/**
 * kmfolderimap.cpp
 *
 * Copyright (c) 2001 Kurt Granroth <granroth@kde.org>
 * Copyright (c) 2000-2002 Michael Haeckel <haeckel@kde.org>
 *
 * This file is based on kmacctimap.coo by Michael Haeckel which was
 * based on popaccount.cpp by Don Sanders
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "kmfolder.h"
#include "kmfolderimap.h"
#include "kmfoldermbox.h"
#include "kmfoldertree.h"
#include "kmmsgdict.h"
#include "undostack.h"
#include "kmfoldermgr.h"
#include "kmkernel.h"
#include "accountmanager.h"
#include "kmfiltermgr.h"
#include "kmmsgdict.h"
#include "imapaccountbase.h"
using KMail::ImapAccountBase;
#include "imapjob.h"
using KMail::ImapJob;
#include "attachmentstrategy.h"
using KMail::AttachmentStrategy;
#include "progressmanager.h"
using KPIM::ProgressItem;
using KPIM::ProgressManager;
#include "listjob.h"
using KMail::ListJob;
#include "kmsearchpattern.h"
#include "searchjob.h"
using KMail::SearchJob;
#include "renamejob.h"
using KMail::RenameJob;

#include <kdebug.h>
#include <kio/scheduler.h>
#include <kconfig.h>

#include <qbuffer.h>
#include <qtextcodec.h>
#include <qstylesheet.h>

#include <assert.h>

KMFolderImap::KMFolderImap(KMFolder* folder, const char* aName)
  : KMFolderMbox(folder, aName),
    mUploadAllFlags( false )
{
  mContentState = imapNoInformation;
  mSubfolderState = imapNoInformation;
  mAccount = 0;
  mIsSelected = false;
  mLastUid = 0;
  mCheckFlags = true;
  mCheckMail = true;
  mCheckingValidity = false;
  mUserRights = 0;
  mAlreadyRemoved = false;
  mHasChildren = ChildrenUnknown;
  mMailCheckProgressItem = 0;
  mListDirProgressItem = 0;
  mAddMessageProgressItem = 0;
  mReadOnly = false;
  mNewMailUidNext = 0;
  mNewMailCount = 0;
  mStatusUidNext = 0;
  mListingsStarted = 0;
  mListingUidNext = 0;
  mListedNumber = 0;
  mListedUidNext = 0;
  mListingExists = -1;
  mIndexComplete = false;
  mListingFromComplete = false;

  connect (this, SIGNAL( folderComplete( KMFolderImap*, bool ) ),
           this, SLOT( slotCompleteMailCheckProgress()) );
}

KMFolderImap::~KMFolderImap()
{
  if (mAccount) {
    mAccount->removeSlaveJobsForFolder( folder() );
    /* Now that we've removed ourselves from the accounts jobs map, kill all
       ongoing operations and reset mailcheck if we were deleted during an
       ongoing mailcheck of our account. Not very gracefull, but safe, and the
       only way I can see to reset the account state cleanly. */
    if ( mAccount->checkingMail( folder() ) ) {
       mAccount->killAllJobs();
    }
  }
  writeConfig();
  if (kmkernel->undoStack()) kmkernel->undoStack()->folderDestroyed( folder() );
  mMetaDataMap.setAutoDelete( true );
  mMetaDataMap.clear();
  mUidMetaDataMap.setAutoDelete( true );
  mUidMetaDataMap.clear();
}


//-----------------------------------------------------------------------------
void KMFolderImap::reallyDoClose(const char* owner)
{
  if (isSelected()) {
      kdWarning(5006) << "Trying to close the selected folder " << label() <<
          " - ignoring!" << endl;
      return;
  }

  // FIXME is this still needed?
  if (account())
    account()->ignoreJobsForFolder( folder() );
  int idx = count();
  while (--idx >= 0) {
    if ( mMsgList[idx]->isMessage() ) {
      KMMessage *msg = static_cast<KMMessage*>(mMsgList[idx]);
      if (msg->transferInProgress())
          msg->setTransferInProgress( false );
    }
  }
  KMFolderMbox::reallyDoClose( owner );
}

KMFolder* KMFolderImap::trashFolder() const
{
  QString trashStr = account()->trash();
  return kmkernel->imapFolderMgr()->findIdString( trashStr );
}

//-----------------------------------------------------------------------------
KMMessage* KMFolderImap::getMsg(int idx)
{
  if(!(idx >= 0 && idx <= count()))
    return 0;

  KMMsgBase* mb = getMsgBase(idx);
  if (!mb) return 0;
  if (mb->isMessage())
  {
    return ((KMMessage*)mb);
  } else {
    KMMessage* msg = FolderStorage::getMsg( idx );
    if ( msg ) // set it incomplete as the msg was not transferred from the server
      msg->setComplete( false );
    return msg;
  }
}

//-----------------------------------------------------------------------------
KMAcctImap* KMFolderImap::account() const
{
  if ( !mAccount ) {
    KMFolderDir *parentFolderDir = dynamic_cast<KMFolderDir*>( folder()->parent() );
    if ( !parentFolderDir ) {
      kdWarning() << k_funcinfo << "No parent folder dir found for " << name() << endl;
      return 0;
    }
    KMFolder *parentFolder = parentFolderDir->owner();
    if ( !parentFolder ) {
      kdWarning() << k_funcinfo << "No parent folder found for " << name() << endl;
      return 0;
    }
    KMFolderImap *parentStorage = dynamic_cast<KMFolderImap*>( parentFolder->storage() );
    if ( parentStorage )
      mAccount = parentStorage->account();
  }
  return mAccount;
}

void KMFolderImap::setAccount(KMAcctImap *aAccount)
{
  mAccount = aAccount;
  if( !folder() || !folder()->child() ) return;
  KMFolderNode* node;
  for (node = folder()->child()->first(); node;
       node = folder()->child()->next())
  {
    if (!node->isDir())
      static_cast<KMFolderImap*>(static_cast<KMFolder*>(node)->storage())->setAccount(aAccount);
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::readConfig()
{
  KConfig* config = KMKernel::config();
  KConfigGroupSaver saver(config, "Folder-" + folder()->idString());
  mCheckMail = config->readBoolEntry("checkmail", true);

  mUidValidity = config->readEntry("UidValidity");
  if ( mImapPath.isEmpty() ) {
    setImapPath( config->readEntry("ImapPath") );
  }
  if (QString(name()).upper() == "INBOX" && mImapPath == "/INBOX/")
  {
    folder()->setSystemFolder( true );
    folder()->setLabel( i18n("inbox") );
  }
  mNoContent = config->readBoolEntry("NoContent", false);
  mReadOnly = config->readBoolEntry("ReadOnly", false);
  mUploadAllFlags = config->readBoolEntry( "UploadAllFlags", true );
  mPermanentFlags = config->readNumEntry( "PermanentFlags", 31 /* default flags */ );
  mNewMailUidNext = config->readUnsignedLongNumEntry( "NewMailUidNext", 0 );
  mNewMailUidValidity = config->readEntry( "NewMailUidValidity" );

  KMFolderMbox::readConfig();
}

//-----------------------------------------------------------------------------
void KMFolderImap::writeConfig()
{
  KConfig* config = KMKernel::config();
  KConfigGroupSaver saver(config, "Folder-" + folder()->idString());
  config->writeEntry("checkmail", mCheckMail);
  config->writeEntry("UidValidity", mUidValidity);
  config->writeEntry("ImapPath", mImapPath);
  config->writeEntry("NoContent", mNoContent);
  config->writeEntry("ReadOnly", mReadOnly);
  config->writeEntry( "UploadAllFlags", mUploadAllFlags );
  config->writeEntry( "PermanentFlags", mPermanentFlags );
  config->writeEntry( "NewMailUidNext", mNewMailUidNext );
  config->writeEntry( "NewMailUidValidity", mNewMailUidValidity );
  KMFolderMbox::writeConfig();
}

//-----------------------------------------------------------------------------
void KMFolderImap::remove()
{
  if ( mAlreadyRemoved || !account() )
  {
    // override
    FolderStorage::remove();
    return;
  }
  KURL url = account()->getUrl();
  url.setPath(imapPath());
  if ( account()->makeConnection() == ImapAccountBase::Error ||
       imapPath().isEmpty() )
  {
    emit removed(folder(), false);
    return;
  }
  KIO::SimpleJob *job = KIO::file_delete(url, false);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  ImapAccountBase::jobData jd(url.url());
  jd.progressItem = ProgressManager::createProgressItem(
                      "ImapFolderRemove" + ProgressManager::getUniqueID(),
                      i18n("Removing folder"),
                      i18n( "URL: %1" ).arg( QStyleSheet::escape( folder()->prettyURL() ) ),
                      false,
                      account()->useSSL() || account()->useTLS() );
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          this, SLOT(slotRemoveFolderResult(KIO::Job *)));
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotRemoveFolderResult(KIO::Job *job)
{
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  if (job->error())
  {
    account()->handleJobError( job, i18n("Error while removing a folder.") );
    emit removed(folder(), false);
  } else {
    account()->removeJob(it);
    FolderStorage::remove();
  }

}

//-----------------------------------------------------------------------------
void KMFolderImap::removeMsg(int idx, bool quiet)
{
  if (idx < 0)
    return;

  if (!quiet)
  {
    KMMessage *msg = getMsg(idx);
    deleteMessage(msg);
  }

  mLastUid = 0;
  KMFolderMbox::removeMsg(idx);
}

void KMFolderImap::removeMsg( const QPtrList<KMMessage>& msgList, bool quiet )
{
  if ( msgList.isEmpty() ) return;
  if (!quiet)
    deleteMessage(msgList);

  mLastUid = 0;

  /* Remove the messages from the local store as well.
     We don't call KMFolderInherited::removeMsg(QPtrList<KMMessage>) but
     iterate ourselves, as that would call KMFolderImap::removeMsg(int)
     and not the one from the store we want to be used. */

  QPtrListIterator<KMMessage> it( msgList );
  KMMessage *msg;
  while ( (msg = it.current()) != 0 ) {
    ++it;
    int idx = find(msg);
    assert( idx != -1);
    // ATTENTION port me to maildir
    KMFolderMbox::removeMsg(idx, quiet);
  }
}

//-----------------------------------------------------------------------------
int KMFolderImap::rename( const QString& newName, KMFolderDir *aParent )
{
  if ( !aParent )
    KMFolderMbox::rename( newName );
  kmkernel->folderMgr()->contentsChanged();
  return 0;
}

//-----------------------------------------------------------------------------
void KMFolderImap::addMsgQuiet(KMMessage* aMsg)
{
  KMFolder *aFolder = aMsg->parent();
  Q_UINT32 serNum = 0;
  aMsg->setTransferInProgress( false );
  if (aFolder) {
    serNum = aMsg->getMsgSerNum();
    kmkernel->undoStack()->pushSingleAction( serNum, aFolder, folder() );
    int idx = aFolder->find( aMsg );
    assert( idx != -1 );
    aFolder->take( idx );
  } else {
    kdDebug(5006) << k_funcinfo << "no parent" << endl;
  }
  if ( !account()->hasCapability("uidplus") ) {
    // Remember the status with the MD5 as key
    // so it can be transfered to the new message
    mMetaDataMap.insert( aMsg->msgIdMD5(),
        new KMMsgMetaData(aMsg->status(), serNum) );
  }

  delete aMsg;
  aMsg = 0;
  getFolder();
}

//-----------------------------------------------------------------------------
void KMFolderImap::addMsgQuiet(QPtrList<KMMessage> msgList)
{
  if ( mAddMessageProgressItem )
  {
    mAddMessageProgressItem->setComplete();
    mAddMessageProgressItem = 0;
  }
  KMFolder *aFolder = msgList.first()->parent();
  int undoId = -1;
  bool uidplus = account()->hasCapability("uidplus");
  for ( KMMessage* msg = msgList.first(); msg; msg = msgList.next() )
  {
    if ( undoId == -1 )
      undoId = kmkernel->undoStack()->newUndoAction( aFolder, folder() );
    if ( msg->getMsgSerNum() > 0 )
      kmkernel->undoStack()->addMsgToAction( undoId, msg->getMsgSerNum() );
    if ( !uidplus ) {
      // Remember the status with the MD5 as key
      // so it can be transfered to the new message
      mMetaDataMap.insert( msg->msgIdMD5(),
          new KMMsgMetaData(msg->status(), msg->getMsgSerNum()) );
    }
    msg->setTransferInProgress( false );
  }
  if ( aFolder ) {
    aFolder->take( msgList );
  } else {
    kdDebug(5006) << k_funcinfo << "no parent" << endl;
  }
  msgList.setAutoDelete(true);
  msgList.clear();
  getFolder();
}

//-----------------------------------------------------------------------------
int KMFolderImap::addMsg(KMMessage* aMsg, int* aIndex_ret)
{
  QPtrList<KMMessage> list;
  list.append(aMsg);
  QValueList<int> index;
  int ret = addMsg(list, index);
  aIndex_ret = &index.first();
  return ret;
}

int KMFolderImap::addMsg(QPtrList<KMMessage>& msgList, QValueList<int>& aIndex_ret)
{
  rememberOwnMessages( msgList );
  KMMessage *aMsg = msgList.getFirst();
  KMFolder *msgParent = aMsg->parent();

  ImapJob *imapJob = 0;
  if (msgParent)
  {
    if (msgParent->folderType() == KMFolderTypeImap)
    {
      if (static_cast<KMFolderImap*>(msgParent->storage())->account() == account())
      {
        // make sure the messages won't be deleted while we work with them
        for ( KMMessage* msg = msgList.first(); msg; msg = msgList.next() )
          msg->setTransferInProgress(true);

        if (folder() == msgParent)
        {
          // transfer the whole message, e.g. a draft-message is canceled and re-added to the drafts-folder
          for ( KMMessage* msg = msgList.first(); msg; msg = msgList.next() )
          {
            if (!msg->isComplete())
            {
              int idx = msgParent->find(msg);
              assert(idx != -1);
              msg = msgParent->getMsg(idx);
            }
            imapJob = new ImapJob(msg, ImapJob::tPutMessage, this);
            connect(imapJob, SIGNAL(messageStored(KMMessage*)),
                     SLOT(addMsgQuiet(KMMessage*)));
            connect(imapJob, SIGNAL(result(KMail::FolderJob*)),
                SLOT(slotCopyMsgResult(KMail::FolderJob*)));
            imapJob->start();
          }

        } else {

          // get the messages and the uids
          QValueList<ulong> uids;
          getUids(msgList, uids);

          // get the sets (do not sort the uids)
          QStringList sets = makeSets(uids, false);

          for ( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it )
          {
            // we need the messages that belong to the current set to pass them to the ImapJob
            QPtrList<KMMessage> temp_msgs = splitMessageList(*it, msgList);
            if ( temp_msgs.isEmpty() ) kdDebug(5006) << "Wow! KMFolderImap::splitMessageList() returned an empty list!" << endl;
            imapJob = new ImapJob(temp_msgs, *it, ImapJob::tMoveMessage, this);
            connect(imapJob, SIGNAL(messageCopied(QPtrList<KMMessage>)),
                SLOT(addMsgQuiet(QPtrList<KMMessage>)));
            connect(imapJob, SIGNAL(result(KMail::FolderJob*)),
                SLOT(slotCopyMsgResult(KMail::FolderJob*)));
            imapJob->start();
          }
        }
        return 0;
      }
      else
      {
        // different account, check if messages can be added
        QPtrListIterator<KMMessage> it( msgList );
        KMMessage *msg;
        while ( (msg = it.current()) != 0 )
        {
          ++it;
          int index;
          if (!canAddMsgNow(msg, &index)) {
            aIndex_ret << index;
            msgList.remove(msg);
          } else {
            if (!msg->transferInProgress())
              msg->setTransferInProgress(true);
          }
        }
      }
    } // if imap
  }

  if ( !msgList.isEmpty() )
  {
    // transfer from local folders or other accounts
    QPtrListIterator<KMMessage> it( msgList );
    KMMessage* msg;
    while ( ( msg = it.current() ) != 0 )
    {
      ++it;
      if ( !msg->transferInProgress() )
        msg->setTransferInProgress( true );
    }
    imapJob = new ImapJob( msgList, QString::null, ImapJob::tPutMessage, this );
    if ( !mAddMessageProgressItem && msgList.count() > 1 )
    {
      // use a parent progress if we have more than 1 message
      // otherwise the normal progress is more accurate
      mAddMessageProgressItem = ProgressManager::createProgressItem(
          "Uploading"+ProgressManager::getUniqueID(),
          i18n("Uploading message data"),
          i18n("Destination folder: %1").arg( QStyleSheet::escape( folder()->prettyURL() ) ),
          true,
          account()->useSSL() || account()->useTLS() );
      mAddMessageProgressItem->setTotalItems( msgList.count() );
      connect ( mAddMessageProgressItem, SIGNAL( progressItemCanceled( KPIM::ProgressItem*)),
          account(), SLOT( slotAbortRequested( KPIM::ProgressItem* ) ) );
      imapJob->setParentProgressItem( mAddMessageProgressItem );
    }
    connect( imapJob, SIGNAL( messageCopied(QPtrList<KMMessage>) ),
        SLOT( addMsgQuiet(QPtrList<KMMessage>) ) );
    connect( imapJob, SIGNAL(result(KMail::FolderJob*)),
             SLOT(slotCopyMsgResult(KMail::FolderJob*)) );
    imapJob->start();
  }

  return 0;
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotCopyMsgResult( KMail::FolderJob* job )
{
  kdDebug(5006) << k_funcinfo << job->error() << endl;
  if ( job->error() ) // getFolder() will not be called in this case
    emit folderComplete( this, false );
}

//-----------------------------------------------------------------------------
void KMFolderImap::copyMsg(QPtrList<KMMessage>& msgList)
{
  rememberOwnMessages( msgList );
  if ( !account()->hasCapability("uidplus") ) {
    for ( KMMessage *msg = msgList.first(); msg; msg = msgList.next() ) {
      // Remember the status with the MD5 as key
      // so it can be transfered to the new message
      mMetaDataMap.insert( msg->msgIdMD5(), new KMMsgMetaData(msg->status()) );
    }
  }

  QValueList<ulong> uids;
  getUids(msgList, uids);
  QStringList sets = makeSets(uids, false);
  for ( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it )
  {
    // we need the messages that belong to the current set to pass them to the ImapJob
    QPtrList<KMMessage> temp_msgs = splitMessageList(*it, msgList);

    ImapJob *job = new ImapJob(temp_msgs, *it, ImapJob::tCopyMessage, this);
    connect(job, SIGNAL(result(KMail::FolderJob*)),
            SLOT(slotCopyMsgResult(KMail::FolderJob*)));
    job->start();
  }
}

//-----------------------------------------------------------------------------
QPtrList<KMMessage> KMFolderImap::splitMessageList(const QString& set,
                                                   QPtrList<KMMessage>& msgList)
{
  int lastcomma = set.findRev(",");
  int lastdub = set.findRev(":");
  int last = 0;
  if (lastdub > lastcomma) last = lastdub;
  else last = lastcomma;
  last++;
  if (last < 0) last = set.length();
  // the last uid of the current set
  const QString last_uid = set.right(set.length() - last);
  QPtrList<KMMessage> temp_msgs;
  QString uid;
  if (!last_uid.isEmpty())
  {
    QPtrListIterator<KMMessage> it( msgList );
    KMMessage* msg = 0;
    while ( (msg = it.current()) != 0 )
    {
      // append the msg to the new list and delete it from the old
      temp_msgs.append(msg);
      uid.setNum( msg->UID() );
      // remove modifies the current
      msgList.remove(msg);
      if (uid == last_uid) break;
    }
  }
  else
  {
    // probably only one element
    temp_msgs = msgList;
  }

  return temp_msgs;
}

//-----------------------------------------------------------------------------
KMMessage* KMFolderImap::take(int idx)
{
  KMMsgBase* mb(mMsgList[idx]);
  if (!mb) return 0;
  if (!mb->isMessage()) readMsg(idx);

  KMMessage *msg = static_cast<KMMessage*>(mb);
  deleteMessage(msg);

  mLastUid = 0;
  return KMFolderMbox::take(idx);
}

void KMFolderImap::take(QPtrList<KMMessage> msgList)
{
  deleteMessage(msgList);

  mLastUid = 0;
  KMFolderMbox::take(msgList);
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotListNamespaces()
{
  disconnect( account(), SIGNAL( connectionResult(int, const QString&) ),
      this, SLOT( slotListNamespaces() ) );
  if ( account()->makeConnection() == ImapAccountBase::Error )
  {
    kdWarning(5006) << "slotListNamespaces - got no connection" << endl;
    return;
  } else if ( account()->makeConnection() == ImapAccountBase::Connecting )
  {
    // wait for the connectionResult
    kdDebug(5006) << "slotListNamespaces - waiting for connection" << endl;
    connect( account(), SIGNAL( connectionResult(int, const QString&) ),
        this, SLOT( slotListNamespaces() ) );
    return;
  }
  kdDebug(5006) << "slotListNamespaces" << endl;
  // reset subfolder states recursively
  setSubfolderState( imapNoInformation );
  mSubfolderState = imapListingInProgress;
  account()->setHasInbox( false );

  ImapAccountBase::ListType type = ImapAccountBase::List;
  if ( account()->onlySubscribedFolders() )
    type = ImapAccountBase::ListSubscribed;

  ImapAccountBase::nsMap map = account()->namespaces();
  QStringList personal = map[ImapAccountBase::PersonalNS];
  // start personal namespace listing and send it directly to slotListResult
  for ( QStringList::Iterator it = personal.begin(); it != personal.end(); ++it )
  {
    KMail::ListJob* job = new KMail::ListJob( account(), type, this,
    account()->addPathToNamespace( *it ) );
    job->setNamespace( *it );
    job->setHonorLocalSubscription( true );
    connect( job, SIGNAL(receivedFolders(const QStringList&, const QStringList&,
            const QStringList&, const QStringList&, const ImapAccountBase::jobData&)),
        this, SLOT(slotListResult(const QStringList&, const QStringList&,
            const QStringList&, const QStringList&, const ImapAccountBase::jobData&)));
    job->start();
  }

  // and now we list all other namespaces and check them ourself
  QStringList ns = map[ImapAccountBase::OtherUsersNS];
  ns += map[ImapAccountBase::SharedNS];
  for ( QStringList::Iterator it = ns.begin(); it != ns.end(); ++it )
  {
    KMail::ListJob* job = new  KMail::ListJob( account(), type, this, account()->addPathToNamespace( *it ) );
    job->setHonorLocalSubscription( true );
    connect( job, SIGNAL(receivedFolders(const QStringList&, const QStringList&,
            const QStringList&, const QStringList&, const ImapAccountBase::jobData&)),
        this, SLOT(slotCheckNamespace(const QStringList&, const QStringList&,
            const QStringList&, const QStringList&, const ImapAccountBase::jobData&)));
    job->start();
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotCheckNamespace( const QStringList& subfolderNames,
                                       const QStringList& subfolderPaths,
                                       const QStringList& subfolderMimeTypes,
                                       const QStringList& subfolderAttributes,
                                       const ImapAccountBase::jobData& jobData )
{
  kdDebug(5006) << "slotCheckNamespace - " << subfolderNames.join(",") << endl;

  // get a correct foldername:
  // strip / and make sure it does not contain the delimiter
  QString name = jobData.path.mid( 1, jobData.path.length()-2 );
  name.remove( account()->delimiterForNamespace( name ) );
  if ( name.isEmpty() ) {
    // happens when an empty namespace is defined
    slotListResult( subfolderNames, subfolderPaths,
        subfolderMimeTypes, subfolderAttributes, jobData );
    return;
  }

  folder()->createChildFolder();
  KMFolderNode *node = 0;
  for ( node = folder()->child()->first(); node;
        node = folder()->child()->next())
  {
    if ( !node->isDir() && node->name() == name )
      break;
  }
  if ( subfolderNames.isEmpty() )
  {
    if ( node )
    {
      kdDebug(5006) << "delete namespace folder " << name << endl;
      KMFolder *fld = static_cast<KMFolder*>(node);
      KMFolderImap* nsFolder = static_cast<KMFolderImap*>(fld->storage());
      nsFolder->setAlreadyRemoved( true );
      kmkernel->imapFolderMgr()->remove( fld );
    }
  } else {
    if ( node )
    {
      // folder exists so pass on the attributes
      kdDebug(5006) << "found namespace folder " << name << endl;
      if ( !account()->listOnlyOpenFolders() )
      {
        KMFolderImap* nsFolder =
          static_cast<KMFolderImap*>(static_cast<KMFolder*>(node)->storage());
        nsFolder->slotListResult( subfolderNames, subfolderPaths,
            subfolderMimeTypes, subfolderAttributes, jobData );
      }
    } else
    {
      // create folder
      kdDebug(5006) << "create namespace folder " << name << endl;
      KMFolder *fld = folder()->child()->createFolder( name );
      if ( fld ) {
        KMFolderImap* f = static_cast<KMFolderImap*> ( fld->storage() );
        f->initializeFrom( this, account()->addPathToNamespace( name ),
            "inode/directory" );
        f->close( "kmfolderimap_create" );
        if ( !account()->listOnlyOpenFolders() )
        {
          f->slotListResult( subfolderNames, subfolderPaths,
              subfolderMimeTypes, subfolderAttributes, jobData );
        }
      }
      kmkernel->imapFolderMgr()->contentsChanged();
    }
  }
}

//-----------------------------------------------------------------------------
bool KMFolderImap::listDirectory()
{
  if ( !account() ||
       ( account() && account()->makeConnection() == ImapAccountBase::Error ) )
  {
    kdDebug(5006) << "KMFolderImap::listDirectory - got no connection" << endl;
    return false;
  }

  if ( this == account()->rootFolder() )
  {
    // a new listing started
    slotListNamespaces();
    return true;
  }
  mSubfolderState = imapListingInProgress;

  // get the folders
  ImapAccountBase::ListType type = ImapAccountBase::List;
  if ( account()->onlySubscribedFolders() )
    type = ImapAccountBase::ListSubscribed;
  KMail::ListJob* job = new  KMail::ListJob( account(), type, this );
  job->setParentProgressItem( account()->listDirProgressItem() );
  job->setHonorLocalSubscription( true );
  connect( job, SIGNAL(receivedFolders(const QStringList&, const QStringList&,
          const QStringList&, const QStringList&, const ImapAccountBase::jobData&)),
      this, SLOT(slotListResult(const QStringList&, const QStringList&,
          const QStringList&, const QStringList&, const ImapAccountBase::jobData&)));
  job->start();

  return true;
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotListResult( const QStringList& subfolderNames,
                                   const QStringList& subfolderPaths,
                                   const QStringList& subfolderMimeTypes,
                                   const QStringList& subfolderAttributes,
                                   const ImapAccountBase::jobData& jobData )
{
  mSubfolderState = imapFinished;
  //kdDebug(5006) << label() << ": folderNames=" << subfolderNames << " folderPaths="
  //<< subfolderPaths << " mimeTypes=" << subfolderMimeTypes << endl;

  // don't react on changes
  kmkernel->imapFolderMgr()->quiet(true);

  bool root = ( this == account()->rootFolder() );
  folder()->createChildFolder();
  if ( root && !account()->hasInbox() )
  {
    // create the INBOX
    initInbox();
  }

  // see if we have a better parent
  // if you have a prefix that contains a folder (e.g "INBOX.") the folders
  // need to be created underneath it
  if ( root && !subfolderNames.empty() )
  {
    KMFolderImap* parent = findParent( subfolderPaths.first(), subfolderNames.first() );
    if ( parent )
    {
      kdDebug(5006) << "KMFolderImap::slotListResult - pass listing to "
        << parent->label() << endl;
      parent->slotListResult( subfolderNames, subfolderPaths,
          subfolderMimeTypes, subfolderAttributes, jobData );
      // cleanup
      QStringList list;
      checkFolders( list, jobData.curNamespace );
      // finish
      emit directoryListingFinished( this );
      kmkernel->imapFolderMgr()->quiet( false );
      return;
    }
  }

  bool emptyList = ( root && subfolderNames.empty() );
  if ( !emptyList )
  {
    checkFolders( subfolderNames, jobData.curNamespace );
  }

  KMFolderImap *f = 0;
  KMFolderNode *node = 0;
  for ( uint i = 0; i < subfolderNames.count(); i++ )
  {
    bool settingsChanged = false;
    // create folders if necessary
    for ( node = folder()->child()->first(); node;
          node = folder()->child()->next() ) {
      if ( !node->isDir() && node->name() == subfolderNames[i] )
        break;
    }
    if ( node ) {
      f = static_cast<KMFolderImap*>(static_cast<KMFolder*>(node)->storage());
    }
    else if ( subfolderPaths[i].upper() != "/INBOX/" )
    {
      kdDebug(5006) << "create folder " << subfolderNames[i] << endl;
      KMFolder *fld = folder()->child()->createFolder(subfolderNames[i]);
      if ( fld ) {
        f = static_cast<KMFolderImap*> ( fld->storage() );
        f->close( "kmfolderimap_create" );
        settingsChanged = true;
      } else {
        kdWarning(5006) << "can't create folder " << subfolderNames[i] << endl;
      }
    }
    if ( f )
    {
      // sanity check
      if ( f->imapPath().isEmpty() ) {
        settingsChanged = true;
      }
      // update progress
      account()->listDirProgressItem()->incCompletedItems();
      account()->listDirProgressItem()->updateProgress();
      account()->listDirProgressItem()->setStatus( folder()->prettyURL() + i18n(" completed") );

      f->initializeFrom( this, subfolderPaths[i], subfolderMimeTypes[i] );
      f->setChildrenState( subfolderAttributes[i] );
      if ( account()->listOnlyOpenFolders() &&
           f->hasChildren() != FolderStorage::ChildrenUnknown )
      {
        settingsChanged = true;
      }

      if ( settingsChanged )
      {
        // tell the tree our information changed
        kmkernel->imapFolderMgr()->contentsChanged();
      }
      if ( ( subfolderMimeTypes[i] == "message/directory" ||
             subfolderMimeTypes[i] == "inode/directory" ) &&
           !account()->listOnlyOpenFolders() )
      {
        f->listDirectory();
      }
    } else {
      kdWarning(5006) << "can't find folder " << subfolderNames[i] << endl;
    }
  } // for subfolders

  // now others should react on the changes
  kmkernel->imapFolderMgr()->quiet( false );
  emit directoryListingFinished( this );
  account()->listDirProgressItem()->setComplete();
}

//-----------------------------------------------------------------------------
void KMFolderImap::initInbox()
{
  KMFolderImap *f = 0;
  KMFolderNode *node = 0;

  for (node = folder()->child()->first(); node;
      node = folder()->child()->next()) {
    if (!node->isDir() && node->name() == "INBOX") break;
  }
  if (node) {
    f = static_cast<KMFolderImap*>(static_cast<KMFolder*>(node)->storage());
  } else {
    f = static_cast<KMFolderImap*>
      (folder()->child()->createFolder("INBOX", true)->storage());
    if ( f )
    {
      f->folder()->setLabel( i18n("inbox") );
      f->close( "kmfolderimap" );
    }
    kmkernel->imapFolderMgr()->contentsChanged();
  }
  if ( f ) {
    f->initializeFrom( this, "/INBOX/", "message/directory" );
    f->setChildrenState( QString::null );
  }
  // so we have an INBOX
  account()->setHasInbox( true );
}

//-----------------------------------------------------------------------------
KMFolderImap* KMFolderImap::findParent( const QString& path, const QString& name )
{
  QString parent = path.left( path.length() - name.length() - 2 );
  if ( parent.length() > 1 )
  {
    // extract name of the parent
    parent = parent.right( parent.length() - 1 );
    if ( parent != label() )
    {
      KMFolderNode *node = folder()->child()->first();
      // look for a better parent
      while ( node )
      {
        if ( node->name() == parent )
        {
          KMFolder* fld = static_cast<KMFolder*>(node);
          KMFolderImap* imapFld = static_cast<KMFolderImap*>( fld->storage() );
          return imapFld;
        }
        node = folder()->child()->next();
      }
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------
void KMFolderImap::checkFolders( const QStringList& subfolderNames,
    const QString& myNamespace )
{
  QPtrList<KMFolder> toRemove;
  KMFolderNode *node = folder()->child()->first();
  while ( node )
  {
    if ( !node->isDir() && subfolderNames.findIndex(node->name()) == -1 )
    {
      KMFolder* fld = static_cast<KMFolder*>(node);
      KMFolderImap* imapFld = static_cast<KMFolderImap*>( fld->storage() );
      // as more than one namespace can be listed in the root folder we need to make sure
      // that the folder is within the current namespace
      bool isInNamespace = ( myNamespace.isEmpty() ||
          myNamespace == account()->namespaceForFolder( imapFld ) );
      kdDebug(5006) << node->name() << " in namespace " << myNamespace << ":" <<
        isInNamespace << endl;
      // ignore some cases
      QString name = node->name();
      bool ignore = ( ( this == account()->rootFolder() ) &&
          ( imapFld->imapPath() == "/INBOX/" ||
            account()->isNamespaceFolder( name ) ||
	    !isInNamespace ) );
      // additional sanity check for broken folders
      if ( imapFld->imapPath().isEmpty() ) {
        ignore = false;
      }
      if ( !ignore )
      {
        // remove the folder without server round trip
        kdDebug(5006) << "checkFolders - " << node->name() << " disappeared" << endl;
        imapFld->setAlreadyRemoved( true );
        toRemove.append( fld );
      } else {
        kdDebug(5006) << "checkFolders - " << node->name() << " ignored" << endl;
      }
    }
    node = folder()->child()->next();
  }
  // remove folders
  for ( KMFolder* doomed=toRemove.first(); doomed; doomed = toRemove.next() )
    kmkernel->imapFolderMgr()->remove( doomed );
}

//-----------------------------------------------------------------------------
void KMFolderImap::initializeFrom( KMFolderImap* parent, QString folderPath,
                                   QString mimeType )
{
  setAccount( parent->account() );
  setImapPath( folderPath );
  setNoContent( mimeType == "inode/directory" );
  setNoChildren( mimeType == "message/digest" );
}

//-----------------------------------------------------------------------------
void KMFolderImap::setChildrenState( QString attributes )
{
  // update children state
  if ( attributes.find( "haschildren", 0, false ) != -1 )
  {
    setHasChildren( FolderStorage::HasChildren );
  } else if ( attributes.find( "hasnochildren", 0, false ) != -1 ||
              attributes.find( "noinferiors", 0, false ) != -1 )
  {
    setHasChildren( FolderStorage::HasNoChildren );
  } else
  {
    if ( account()->listOnlyOpenFolders() ) {
      setHasChildren( FolderStorage::HasChildren );
    } else {
      setHasChildren( FolderStorage::ChildrenUnknown );
    }
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::checkValidity()
{
  if (!account()) {
    emit folderComplete(this, false);
    close("checkvalidity");
    return;
  }
  KURL url = account()->getUrl();
  url.setPath(imapPath() + ";UID=0:0");
  kdDebug(5006) << "KMFolderImap::checkValidity of: " << imapPath() << endl;

  // Start with a clean slate
  disconnect( account(), SIGNAL( connectionResult(int, const QString&) ),
              this, SLOT( checkValidity() ) );

  KMAcctImap::ConnectionState connectionState = account()->makeConnection();
  if ( connectionState == ImapAccountBase::Error ) {
    kdDebug(5006) << "KMFolderImap::checkValidity - got no connection" << endl;
    emit folderComplete(this, false);
    mContentState = imapNoInformation;
    close("checkvalidity");
    return;
  } else if ( connectionState == ImapAccountBase::Connecting ) {
    // We'll wait for the connectionResult signal from the account. If it
    // errors, the above will catch it.
    kdDebug(5006) << "CheckValidity - waiting for connection" << endl;
    connect( account(), SIGNAL( connectionResult(int, const QString&) ),
        this, SLOT( checkValidity() ) );
    return;
  }
  // Only check once at a time.
  if (mCheckingValidity) {
    kdDebug(5006) << "KMFolderImap::checkValidity - already checking" << endl;
    close("checkvalidity");
    return;
  }
  // otherwise we already are inside a mailcheck
  if ( !mMailCheckProgressItem ) {
    ProgressItem* parent = ( account()->checkingSingleFolder() ? 0 :
        account()->mailCheckProgressItem() );
    mMailCheckProgressItem = ProgressManager::createProgressItem(
              parent,
              "MailCheck" + folder()->prettyURL(),
              QStyleSheet::escape( folder()->prettyURL() ),
              i18n("checking"),
              false,
              account()->useSSL() || account()->useTLS() );
  } else {
    mMailCheckProgressItem->setProgress(0);
  }
  if ( account()->mailCheckProgressItem() ) {
    account()->mailCheckProgressItem()->setStatus( folder()->prettyURL() );
  }
  ImapAccountBase::jobData jd( url.url() );
  KIO::SimpleJob *job = KIO::get(url, false, false);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          SLOT(slotCheckValidityResult(KIO::Job *)));
  connect(job, SIGNAL(data(KIO::Job *, const QByteArray &)),
          SLOT(slotSimpleData(KIO::Job *, const QByteArray &)));
  // Only check once at a time.
  mCheckingValidity = true;
}


//-----------------------------------------------------------------------------
ulong KMFolderImap::lastUid()
{
  if ( mLastUid > 0 )
      return mLastUid;
  open("lastuid");
  if (count() > 0)
  {
    KMMsgBase * base = getMsgBase(count()-1);
    mLastUid = base->UID();
  }
  close("lastuid");
  return mLastUid;
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotCheckValidityResult(KIO::Job * job)
{
  kdDebug(5006) << "KMFolderImap::slotCheckValidityResult of: " << fileName() << endl;
  mCheckingValidity = false;
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  if (job->error()) {
    if ( job->error() != KIO::ERR_ACCESS_DENIED ) {
      // we suppress access denied messages because they are normally a result of
      // explicitely set ACLs. Do not save this information (e.g. setNoContent) so that
      // we notice when this changes
      account()->handleJobError( job, i18n("Error while querying the server status.") );
    }
    mContentState = imapNoInformation;
    emit folderComplete(this, false);
    close("checkvalidity");
  } else {
    QCString cstr((*it).data.data(), (*it).data.size() + 1);
    int a = cstr.find("X-uidValidity: ");
    int b = cstr.find("\r\n", a);
    QString uidv;
    if ( (b - a - 15) >= 0 )
        uidv = cstr.mid(a + 15, b - a - 15);
    a = cstr.find("X-Access: ");
    b = cstr.find("\r\n", a);
    QString access;
    if ( (b - a - 10) >= 0 )
        access = cstr.mid(a + 10, b - a - 10);
    mReadOnly = access == "Read only";
    a = cstr.find("X-Count: ");
    b = cstr.find("\r\n", a);
    int exists = -1;
    bool ok = false;
    if ( (b - a - 9) >= 0 )
        exists = cstr.mid(a + 9, b - a - 9).toInt(&ok);
    if ( !ok ) exists = -1;
    a = cstr.find( "X-PermanentFlags: " );
    b = cstr.find( "\r\n", a );
    if ( a >= 0 && (b - a - 18) >= 0 )
      mPermanentFlags = cstr.mid( a + 18, b - a - 18 ).toInt(&ok);
    if ( !ok ) mPermanentFlags = 0;
    // the UID the next message will get, 0 if unknown: only that of a
    // SELECT of this check (X-Selected), which also gave the count.  The
    // UIDNEXT of an earlier SELECT may be stale: a NOOP since then can
    // report new and expunged messages without repeating it.
    ulong uidNext = 0;
    a = cstr.find( "X-UidNext: " );
    b = cstr.find( "\r\n", a );
    if ( a >= 0 && (b - a - 11) >= 0 && cstr.find( "\r\nX-Selected: now\r\n" ) >= 0 ) {
      uidNext = cstr.mid( a + 11, b - a - 11 ).toULong( &ok );
      if ( !ok ) uidNext = 0;
    }
    // A listing of the messages starts: its SELECT, the next UID of
    // this one or (the mailbox selected before) of the one before,
    // lower: a bound of the UIDs it lists as good.  The index is not
    // complete until it ends (killed, failed: not).
    ++mListingsStarted;
    mListingUidValidity = uidv;
    mListingUidNext = uidNext;
    if ( !mListingUidNext && a >= 0 && (b - a - 11) >= 0 ) {
      mListingUidNext = cstr.mid( a + 11, b - a - 11 ).toULong( &ok );
      if ( !ok ) mListingUidNext = 0;
    }
    mListingExists = exists;
    mListingFromComplete = mIndexComplete;
    mIndexComplete = false;
    QString startUid;
    if (uidValidity() != uidv)
    {
      // uidValidity changed
      kdDebug(5006) << k_funcinfo << "uidValidty changed from "
       << uidValidity() << " to " << uidv << endl;
      if ( !uidValidity().isEmpty() )
      {
        account()->ignoreJobsForFolder( folder() );
        mUidMetaDataMap.clear();
      }
      mLastUid = 0;
      setUidValidity(uidv);
      writeConfig();
    } else {
      if (!mCheckFlags)
        startUid = QString::number(lastUid() + 1);
    }
    account()->removeJob(it);
    if ( mMailCheckProgressItem )
    {
      if ( startUid.isEmpty() ) {
        // flags for all messages are loaded
        mMailCheckProgressItem->setTotalItems( exists );
      } else {
        // only an approximation but doesn't hurt
        int remain = exists - count();
        if ( remain < 0 ) remain = 1;
        mMailCheckProgressItem->setTotalItems( remain );
      }
      mMailCheckProgressItem->setCompletedItems( 0 );
    }
    // Nothing new: the server has as many messages as the folder, and none
    // got a UID after the last one known.  The envelopes of "UID last+1:*"
    // would only be the last message's, which is known: don't ask for them.
    // (With fewer messages on the server, the fetch notices it and reloads.)
    if ( !startUid.isEmpty() && uidNext > 0 && exists >= 0 && exists == count()
         && uidNext <= startUid.toULong() ) {
      quiet( true ); // as reallyGetFolder(), for finishMailCheck()
      mContentState = imapDownloadInProgress;
      finishMailCheck( "getMessage", imapFinished );
      return;
    }
    reallyGetFolder(startUid);
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::getAndCheckFolder(bool force)
{
  if (mNoContent)
    return getFolder(force);

  if ( account() )
    account()->processNewMailSingleFolder( folder() );
  if (force) {
    // force an update
    mCheckFlags = true;
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::getFolder(bool force)
{
  mGuessedUnreadMsgs = -1;
  if (mNoContent)
  {
    mContentState = imapFinished;
    emit folderComplete(this, true);
    return;
  }
  open("getfolder");
  mContentState = imapListingInProgress;
  if (force) {
    // force an update
    mCheckFlags = true;
  }
  checkValidity();
}


//-----------------------------------------------------------------------------
void KMFolderImap::reallyGetFolder(const QString &startUid)
{
  KURL url = account()->getUrl();
  if ( account()->makeConnection() != ImapAccountBase::Connected )
  {
    mContentState = imapNoInformation;
    emit folderComplete(this, false);
    close("listfolder");
    return;
  }
  quiet(true);
  if (startUid.isEmpty())
  {
    // (all the messages: the index complete when it ends)
    mListingFromComplete = true;
    if ( mMailCheckProgressItem )
      mMailCheckProgressItem->setStatus( i18n("Retrieving message status") );
    url.setPath(imapPath() + ";SECTION=UID FLAGS");
    KIO::SimpleJob *job = KIO::listDir(url, false);
    KIO::Scheduler::assignJobToSlave(account()->slave(), job);
    ImapAccountBase::jobData jd( url.url(), folder() );
    jd.cancellable = true;
    account()->insertJob(job, jd);
    connect(job, SIGNAL(result(KIO::Job *)),
            this, SLOT(slotListFolderResult(KIO::Job *)));
    connect(job, SIGNAL(entries(KIO::Job *, const KIO::UDSEntryList &)),
            this, SLOT(slotListFolderEntries(KIO::Job *,
            const KIO::UDSEntryList &)));
  } else {
    mContentState = imapDownloadInProgress;
    if ( mMailCheckProgressItem )
      mMailCheckProgressItem->setStatus( i18n("Retrieving messages") );
    url.setPath(imapPath() + ";UID=" + startUid
      + ":*;SECTION=ENVELOPE");
    KIO::SimpleJob *newJob = KIO::get(url, false, false);
    KIO::Scheduler::assignJobToSlave(account()->slave(), newJob);
    ImapAccountBase::jobData jd( url.url(), folder() );
    jd.cancellable = true;
    account()->insertJob(newJob, jd);
    connect(newJob, SIGNAL(result(KIO::Job *)),
            this, SLOT(slotGetLastMessagesResult(KIO::Job *)));
    connect(newJob, SIGNAL(data(KIO::Job *, const QByteArray &)),
            this, SLOT(slotGetMessagesData(KIO::Job *, const QByteArray &)));
  }
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotListFolderResult(KIO::Job * job)
{
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  QString uids;
  if (job->error())
  {
    account()->handleJobError( job,
         i18n("Error while listing the contents of the folder %1.").arg( label() ) );
    account()->removeJob(it);
    finishMailCheck( "listfolder", imapNoInformation );
    return;
  }
  mCheckFlags = false;
  QStringList::Iterator uid;
  /*
    The code below does the following:
    - for each mail in the local store and each entry we got from the server,
      compare the local uid with the one from the server and update the status
      flags of the mails
    - for all mails that are not already locally present, start a job which
      gets the envelope of each
    - remove all locally present mails if the server does not list them anymore
  */
  if ( count() ) {
    int idx = 0, c, serverFlags;
    ulong mailUid, serverUid;
    uid = (*it).items.begin();
    while ( idx < count() && uid != (*it).items.end() ) {
      KMMsgBase *msgBase = getMsgBase( idx );
      mailUid = msgBase->UID();
      // parse the uid from the server and the flags out of the list from
      // the server. Format: 1234, 1
      c = (*uid).find(",");
      serverUid = (*uid).left( c ).toLong();
      serverFlags = (*uid).mid( c+1 ).toInt();
      if ( mailUid < serverUid ) {
        removeMsg( idx, true );
      } else if ( mailUid == serverUid ) {
        // if this is a read only folder, ignore status updates from the server
        // since we can't write our status back our local version is what has to
        // be considered correct.
        if ( !mReadOnly || !GlobalSettings::allowLocalFlags() ) {
          int supportedFlags = mUploadAllFlags ? 31 : mPermanentFlags;
          if ( mReadOnly )
            supportedFlags = INT_MAX;
          flagsToStatus( msgBase, serverFlags, false, supportedFlags );
        } else
          seenFlagToStatus( msgBase, serverFlags, false );
        uid = (*it).items.remove(uid);
        // the index is known: don't make getMsgSerNum() search the folder
        if ( msgBase->getMsgSerNum( idx ) > 0 ) {
          saveMsgMetaData( static_cast<KMMessage*>(msgBase) );
        }
        idx++;
      }
      else break;  // happens only, if deleted mails reappear on the server
    }
    // remove all remaining entries in the local cache, they are no longer
    // present on the server
    while (idx < count()) removeMsg(idx, true);
  }
  // strip the flags from the list of uids, so it can be reused
  for (uid = (*it).items.begin(); uid != (*it).items.end(); ++uid)
    (*uid).truncate((*uid).find(","));
  ImapAccountBase::jobData jd( QString::null, (*it).parent );
  jd.total = (*it).items.count();
  if (jd.total == 0)
  {
    finishMailCheck( "listfolder", imapFinished );
    account()->removeJob(it);
    return;
  }
  if ( mMailCheckProgressItem )
  {
    // next step for the progressitem
    mMailCheckProgressItem->setCompletedItems( 0 );
    mMailCheckProgressItem->setTotalItems( jd.total );
    mMailCheckProgressItem->setProgress( 0 );
    mMailCheckProgressItem->setStatus( i18n("Retrieving messages") );
  }

  QStringList sets;
  uid = (*it).items.begin();
  if (jd.total == 1) sets.append(*uid + ":" + *uid);
  else sets = makeSets( (*it).items );
  account()->removeJob(it); // don't use *it below

  // Now kick off the getting of envelopes for the new mails in the folder
  for (QStringList::Iterator i = sets.begin(); i != sets.end(); ++i)
  {
    mContentState = imapDownloadInProgress;
    KURL url = account()->getUrl();
    url.setPath(imapPath() + ";UID=" + *i + ";SECTION=ENVELOPE");
    KIO::SimpleJob *newJob = KIO::get(url, false, false);
    jd.url = url.url();
    KIO::Scheduler::assignJobToSlave(account()->slave(), newJob);
    account()->insertJob(newJob, jd);
    connect(newJob, SIGNAL(result(KIO::Job *)),
        this, (i == sets.at(sets.count() - 1))
        ? SLOT(slotGetLastMessagesResult(KIO::Job *))
        : SLOT(slotGetMessagesResult(KIO::Job *)));
    connect(newJob, SIGNAL(data(KIO::Job *, const QByteArray &)),
        this, SLOT(slotGetMessagesData(KIO::Job *, const QByteArray &)));
  }
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotListFolderEntries(KIO::Job * job,
  const KIO::UDSEntryList & uds)
{
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  QString mimeType, name;
  long int flags = 0;
  for (KIO::UDSEntryList::ConstIterator udsIt = uds.begin();
    udsIt != uds.end(); udsIt++)
  {
    for (KIO::UDSEntry::ConstIterator eIt = (*udsIt).begin();
      eIt != (*udsIt).end(); eIt++)
    {
      if ((*eIt).m_uds == KIO::UDS_NAME)
        name = (*eIt).m_str;
      else if ((*eIt).m_uds == KIO::UDS_MIME_TYPE)
        mimeType = (*eIt).m_str;
      else if ((*eIt).m_uds == KIO::UDS_ACCESS)
        flags = (*eIt).m_long;
    }
    if ((mimeType == "message/rfc822-imap" || mimeType == "message/rfc822") &&
        !(flags & 8)) {
      (*it).items.append(name + "," + QString::number(flags));
      if ( mMailCheckProgressItem ) {
        mMailCheckProgressItem->incCompletedItems();
        mMailCheckProgressItem->updateProgress();
      }
    }
  }
}


// debugging helper
//X static QString flagsToString( int flags )
//X {
//X     QString str("(");
//X     if ( flags & 4 ) {
//X         str += "\\Flagged ";
//X     }
//X     if ( flags & 2 ) {
//X         str += "\\Answered ";
//X     }
//X     if ( flags & 1 ) {
//X         str += "\\Seen";
//X     }
//X     str += ")";
//X     return str;
//X }

//-----------------------------------------------------------------------------
void KMFolderImap::flagsToStatus(KMMsgBase *msg, int flags, bool newMsg, int supportedFlags )
{
  if ( !msg ) return;

  // see imap4/imapinfo.h for the magic numbers
  static const struct {
    const int imapFlag;
    const int kmFlag;
    const bool standardFlag;
  } imapFlagMap[] = {
    { 2, KMMsgStatusReplied, true },
    { 4, KMMsgStatusFlag, true },
    { 128, KMMsgStatusForwarded, false },
    { 256, KMMsgStatusTodo, false },
    { 512, KMMsgStatusWatched, false },
    { 1024, KMMsgStatusIgnored, false }
  };
  static const int numFlags = sizeof imapFlagMap / sizeof *imapFlagMap;

  const KMMsgStatus oldStatus = msg->status();
  for ( int i = 0; i < numFlags; ++i ) {
    if ( ( (supportedFlags & imapFlagMap[i].imapFlag) == 0 && (supportedFlags & 64) == 0 )
         && !imapFlagMap[i].standardFlag ) {
      continue;
    }
    if ( ((flags & imapFlagMap[i].imapFlag) > 0) != ((oldStatus & imapFlagMap[i].kmFlag) > 0) ) {
      msg->toggleStatus( imapFlagMap[i].kmFlag );
    }
  }

  seenFlagToStatus( msg, flags, newMsg );
}

void KMFolderImap::seenFlagToStatus(KMMsgBase * msg, int flags, bool newMsg)
{
  if ( !msg ) return;

  const KMMsgStatus oldStatus = msg->status();
  if ( (flags & 1) && (oldStatus & KMMsgStatusOld) == 0 )
    msg->setStatus( KMMsgStatusOld );

  // In case the message does not have the seen flag set, override our local
  // notion that it is read. Otherwise the count of unread messages and the
  // number of messages which actually show up as read can go out of sync.
  if ( msg->isOfUnknownStatus() || (!(flags&1) && !(oldStatus&(KMMsgStatusNew|KMMsgStatusUnread)) ) ) {
    if (newMsg) {
      if ( (oldStatus & KMMsgStatusNew) == 0 )
        msg->setStatus( KMMsgStatusNew );
    } else {
      if ( (oldStatus & KMMsgStatusUnread) == 0 )
        msg->setStatus( KMMsgStatusUnread );
    }
  }
}


//-----------------------------------------------------------------------------
QString KMFolderImap::statusToFlags(KMMsgStatus status, int supportedFlags)
{
  QString flags;
  if (status & KMMsgStatusDeleted)
    flags = "\\DELETED";
  else {
    if (status & KMMsgStatusOld || status & KMMsgStatusRead)
      flags = "\\SEEN ";
    if (status & KMMsgStatusReplied)
      flags += "\\ANSWERED ";
    if (status & KMMsgStatusFlag)
      flags += "\\FLAGGED ";
    // non standard flags
    if ( (status & KMMsgStatusForwarded) && ((supportedFlags & 64) || (supportedFlags & 128)) )
      flags += "$FORWARDED ";
    if ( (status & KMMsgStatusTodo) && ((supportedFlags & 64) || (supportedFlags & 256)) )
      flags += "$TODO ";
    if ( (status & KMMsgStatusWatched) && ((supportedFlags & 64) || (supportedFlags & 512)) )
      flags += "$WATCHED ";
    if ( (status & KMMsgStatusIgnored) && ((supportedFlags & 64) || (supportedFlags & 1024)) )
      flags += "$IGNORED ";
  }

  return flags.simplifyWhiteSpace();
}

//-------------------------------------------------------------
void
KMFolderImap::ignoreJobsForMessage( KMMessage* msg )
{
  if ( !msg || msg->transferInProgress() ||
       !msg->parent() || msg->parent()->folderType() != KMFolderTypeImap )
    return;
  KMAcctImap *account;
  if ( !(account = static_cast<KMFolderImap*>(msg->storage())->account()) )
    return;

  account->ignoreJobsForMessage( msg );
}

//-------------------------------------------------------------
QMap<QString, int> KMFolderImap::retrievingParts( const KMMessage * msg ) const
{
  KMAcctImap * acct = account();
  return acct ? acct->retrievingParts( msg ) : QMap<QString, int>();
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotGetMessagesData(KIO::Job * job, const QByteArray & data)
{
  if ( data.isEmpty() ) return; // optimization
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  (*it).cdata += QCString(data, data.size() + 1);
  int pos = (*it).cdata.find("\r\n--IMAPDIGEST");
  if ( pos == -1 ) {
    // if we do not find the pattern in the complete string we will not find
    // it in a substring.
    return;
  }
  if (pos > 0)
  {
    int p = (*it).cdata.find("\r\nX-uidValidity:");
    if (p != -1) setUidValidity((*it).cdata
      .mid(p + 17, (*it).cdata.find("\r\n", p+1) - p - 17));
    int c = (*it).cdata.find("\r\nX-Count:");
    if ( c != -1 )
    {
      bool ok;
      int exists = (*it).cdata.mid( c+10,
          (*it).cdata.find("\r\n", c+1) - c-10 ).toInt(&ok);
      if ( ok && exists < count() ) {
        kdDebug(5006) << "KMFolderImap::slotGetMessagesData - server has less messages (" <<
          exists << ") then folder (" << count() << "), so reload" << endl;
        open("getMessage");
        reallyGetFolder( QString::null );
        (*it).cdata.remove(0, pos);
        return;
      } else if ( ok ) {
        int delta = exists - count();
        if ( mMailCheckProgressItem ) {
          mMailCheckProgressItem->setTotalItems( delta );
        }
      }
    }
    (*it).cdata.remove(0, pos);
  }
  pos = (*it).cdata.find("\r\n--IMAPDIGEST", 1);
  int flags;
  while (pos >= 0)
  {
    KMMessage *msg = new KMMessage;
    msg->setComplete( false );
    msg->setReadyToShow( false );
    // nothing between the boundaries, older UWs do that
    if ( pos != 14 ) {
      msg->fromString( (*it).cdata.mid(16, pos - 16) );
      flags = msg->headerField("X-Flags").toInt();
      ulong uid = msg->UID();
      KMMsgMetaData *md =  0;
      if ( mUidMetaDataMap.find( uid ) ) {
          md =  mUidMetaDataMap[uid];
      }
      ulong serNum = 0;
      if ( md ) {
        serNum = md->serNum();
      }
      bool ok = true;
      if ( uid <= lastUid() && serNum > 0 ) {
        // the UID is already known so no need to create it
        ok = false;
      }
      // deleted flag
      if ( flags & 8 )
        ok = false;
      if ( !ok ) {
        delete msg;
        msg = 0;
      } else {
        if ( serNum > 0 ) {
          // assign the sernum from the cache
          msg->setMsgSerNum( serNum );
        }
        // Transfer the status, if it is cached.
        if ( md ) {
          msg->setStatus( md->status() );
        } else if ( !account()->hasCapability("uidplus") ) {
          // see if we have cached the msgIdMD5 and get the status +
          // serial number from there
          QString id = msg->msgIdMD5();
          if ( mMetaDataMap.find( id ) ) {
            md =  mMetaDataMap[id];
            msg->setStatus( md->status() );
            if ( md->serNum() != 0 && serNum == 0 ) {
              msg->setMsgSerNum( md->serNum() );
            }
            mMetaDataMap.remove( id );
            delete md;
          }
        }
        KMFolderMbox::addMsg(msg, 0);
        // Merge with the flags from the server.
        flagsToStatus((KMMsgBase*)msg, flags, true, mUploadAllFlags ? 31 : mPermanentFlags);
        // (new mail: the messages the count of the check accepts,
        // slotNewMailFetchResult(), not every message listed)
        // set the correct size
        msg->setMsgSizeServer( msg->headerField("X-Length").toUInt() );
        msg->setUID(uid);
        if ( msg->getMsgSerNum() > 0 ) {
          saveMsgMetaData( msg );
        }
        // Filter messages that have arrived in the inbox folder
        if ( folder()->isSystemFolder() && imapPath() == "/INBOX/"
            && kmkernel->filterMgr()->atLeastOneIncomingFilterAppliesTo( account()->id() ) )
            account()->execFilters( msg->getMsgSerNum() );

        if ( count() > 1 ) {
          unGetMsg(count() - 1);
        }
        mLastUid = uid;
        if ( mMailCheckProgressItem ) {
          mMailCheckProgressItem->incCompletedItems();
          mMailCheckProgressItem->updateProgress();
        }
      }
    }
    (*it).cdata.remove(0, pos);
    (*it).done++;
    pos = (*it).cdata.find("\r\n--IMAPDIGEST", 1);
  } // while
}

//-------------------------------------------------------------
FolderJob*
KMFolderImap::doCreateJob( KMMessage *msg, FolderJob::JobType jt,
                           KMFolder *folder, QString partSpecifier,
                           const AttachmentStrategy *as ) const
{
  KMFolderImap* kmfi = folder? dynamic_cast<KMFolderImap*>(folder->storage()) : 0;
  if ( jt == FolderJob::tGetMessage && partSpecifier == "STRUCTURE" &&
       account() && account()->loadOnDemand() &&
       ( msg->msgSizeServer() > 5000 || msg->msgSizeServer() == 0 ) &&
       ( msg->signatureState() == KMMsgNotSigned ||
         msg->signatureState() == KMMsgSignatureStateUnknown ) &&
       ( msg->encryptionState() == KMMsgNotEncrypted ||
         msg->encryptionState() == KMMsgEncryptionStateUnknown ) )
  {
    // load-on-demand: retrieve the BODYSTRUCTURE and to speed things up also the headers
    // this is not activated for small or signed messages
    ImapJob *job = new ImapJob( msg, jt, kmfi, "HEADER" );
    job->start();
    ImapJob *job2 = new ImapJob( msg, jt, kmfi, "STRUCTURE", as );
    job2->start();
    job->setParentFolder( this );
    return job;
  } else {
    // download complete message or part (attachment)
    if ( partSpecifier == "STRUCTURE" ) // hide from outside
      partSpecifier = QString::null;

    ImapJob *job = new ImapJob( msg, jt, kmfi, partSpecifier );
    job->setParentFolder( this );
    return job;
  }
}

//-------------------------------------------------------------
FolderJob*
KMFolderImap::doCreateJob( QPtrList<KMMessage>& msgList, const QString& sets,
                           FolderJob::JobType jt, KMFolder *folder ) const
{
  KMFolderImap* kmfi = dynamic_cast<KMFolderImap*>(folder->storage());
  ImapJob *job = new ImapJob( msgList, sets, jt, kmfi );
  job->setParentFolder( this );
  return job;
}

//-----------------------------------------------------------------------------
void KMFolderImap::getMessagesResult(KIO::Job * job, bool lastSet)
{
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  if (job->error()) {
    account()->handleJobError( job, i18n("Error while retrieving messages.") );
    finishMailCheck( "getMessage", imapNoInformation );
    return;
  }
  if (lastSet) {
    finishMailCheck( "getMessage", imapFinished );
    account()->removeJob(it);
  }
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotGetLastMessagesResult(KIO::Job * job)
{
  getMessagesResult(job, true);
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotGetMessagesResult(KIO::Job * job)
{
  getMessagesResult(job, false);
}


//-----------------------------------------------------------------------------
void KMFolderImap::createFolder(const QString &name, const QString& parentPath,
                                bool askUser)
{
  kdDebug(5006) << "KMFolderImap::createFolder - name=" << name << ",parent=" <<
    parentPath << ",askUser=" << askUser << endl;
  if ( account()->makeConnection() != ImapAccountBase::Connected ) {
    kdWarning(5006) << "KMFolderImap::createFolder - got no connection" << endl;
    return;
  }
  KURL url = account()->getUrl();
  QString parent = ( parentPath.isEmpty() ? imapPath() : parentPath );
  QString path = account()->createImapPath( parent, name );
  if ( askUser ) {
    path += "/;INFO=ASKUSER";
  }
  url.setPath( path );

  KIO::SimpleJob *job = KIO::mkdir(url);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  ImapAccountBase::jobData jd( url.url(), folder() );
  jd.items = name;
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          this, SLOT(slotCreateFolderResult(KIO::Job *)));
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotCreateFolderResult(KIO::Job * job)
{
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;

  QString name;
  if ( it.data().items.count() > 0 )
    name = it.data().items.first();

  if (job->error())
  {
    if ( job->error() == KIO::ERR_COULD_NOT_MKDIR ) {
      // Creating a folder failed, remove it from the tree.
      account()->listDirectory( );
    }
    account()->handleJobError( job, i18n("Error while creating a folder.") );
    emit folderCreationResult( name, false );
  } else {
    listDirectory();
    account()->removeJob(job);
    emit folderCreationResult( name, true );
  }
}


//-----------------------------------------------------------------------------
static QTextCodec *sUtf7Codec = 0;

QTextCodec * KMFolderImap::utf7Codec()
{
  if (!sUtf7Codec) sUtf7Codec = QTextCodec::codecForName("utf-7");
  return sUtf7Codec;
}


//-----------------------------------------------------------------------------
QString KMFolderImap::encodeFileName(const QString &name)
{
  QString result = utf7Codec()->fromUnicode(name);
  return KURL::encode_string_no_slash(result);
}


//-----------------------------------------------------------------------------
QString KMFolderImap::decodeFileName(const QString &name)
{
  QString result = KURL::decode_string(name);
  return utf7Codec()->toUnicode(result.latin1());
}

//-----------------------------------------------------------------------------
bool KMFolderImap::autoExpunge()
{
  if (account())
    return account()->autoExpunge();

  return false;
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotSimpleData(KIO::Job * job, const QByteArray & data)
{
  if ( data.isEmpty() ) return; // optimization
  ImapAccountBase::JobIterator it = account()->findJob(job);
  if ( it == account()->jobsEnd() ) return;
  QBuffer buff((*it).data);
  buff.open(IO_WriteOnly | IO_Append);
  buff.writeBlock(data.data(), data.size());
  buff.close();
}

//-----------------------------------------------------------------------------
void KMFolderImap::deleteMessage(KMMessage * msg)
{
  mUidMetaDataMap.remove( msg->UID() );
  mMetaDataMap.remove( msg->msgIdMD5() );
  KURL url = account()->getUrl();
  KMFolderImap *msg_parent = static_cast<KMFolderImap*>(msg->storage());
  ulong uid = msg->UID();
  /* If the uid is empty the delete job below will nuke all mail in the
     folder, so we better safeguard against that. See ::expungeFolder, as
     to why. :( */
  if ( uid == 0 ) {
     kdDebug( 5006 ) << "KMFolderImap::deleteMessage: Attempt to delete "
                        "an empty UID. Aborting."  << endl;
     return;
  }
  url.setPath(msg_parent->imapPath() + ";UID=" + QString::number(uid) );
  if ( account()->makeConnection() != ImapAccountBase::Connected )
    return;
  KIO::SimpleJob *job = KIO::file_delete(url, false);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  ImapAccountBase::jobData jd( url.url(), 0 );
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          account(), SLOT(slotSimpleResult(KIO::Job *)));
}

void KMFolderImap::deleteMessage(const QPtrList<KMMessage>& msgList)
{
  QPtrListIterator<KMMessage> it( msgList );
  KMMessage *msg;
  while ( (msg = it.current()) != 0 ) {
    ++it;
    mUidMetaDataMap.remove( msg->UID() );
    mMetaDataMap.remove( msg->msgIdMD5() );
  }

  QValueList<ulong> uids;
  getUids(msgList, uids);
  QStringList sets = makeSets(uids);

  KURL url = account()->getUrl();
  KMFolderImap *msg_parent = static_cast<KMFolderImap*>(msgList.getFirst()->storage());
  for ( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it )
  {
    QString uid = *it;
    // Don't delete with no uid, that nukes the folder. Should not happen, but
    // better safe than sorry.
    if ( uid.isEmpty() ) continue;
    url.setPath(msg_parent->imapPath() + ";UID=" + uid);
    if ( account()->makeConnection() != ImapAccountBase::Connected )
      return;
    KIO::SimpleJob *job = KIO::file_delete(url, false);
    KIO::Scheduler::assignJobToSlave(account()->slave(), job);
    ImapAccountBase::jobData jd( url.url(), 0 );
    account()->insertJob(job, jd);
    connect(job, SIGNAL(result(KIO::Job *)),
        account(), SLOT(slotSimpleResult(KIO::Job *)));
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::setStatus(int idx, KMMsgStatus status, bool toggle)
{
  QValueList<int> ids; ids.append(idx);
  setStatus(ids, status, toggle);
}

void KMFolderImap::setStatus(QValueList<int>& _ids, KMMsgStatus status, bool toggle)
{
  FolderStorage::setStatus(_ids, status, toggle);
  QValueList<int> ids;
  if ( mUploadAllFlags ) {
    kdDebug(5006) << k_funcinfo << "Migrating all flags to the server" << endl;
    ids.clear();
    for ( int i = 0; i < count(); ++i )
      ids << i;
    mUploadAllFlags = false;
  } else {
    ids = _ids;
  }

  /* The status has been already set in the local index. Update the flags on
   * the server. To avoid doing that for each message individually, group them
   * by the status string they will be assigned and make sets for each of those
   * groups of mails. This is necessary because the imap kio_slave status job
   * does not append flags but overwrites them. Example:
   *
   * 2 important mails and 3 unimportant mail, all unread. Mark all as read calls
   * this method with a list of uids. The 2 important mails need to get the string
   * \SEEN \FLAGGED while the others need to get just \SEEN. Build sets for each
   * of those and sort them, so the server can handle them efficiently. */

  if ( mReadOnly ) { // mUserRights is not available here
    // FIXME duplicated code in KMFolderCachedImap
    QValueList<ulong> seenUids, unseenUids;
    for ( QValueList<int>::ConstIterator it = ids.constBegin(); it != ids.constEnd(); ++it ) {
      KMMessage *msg = 0;
      bool unget = !isMessage(*it);
      msg = getMsg(*it);
      if (!msg) continue;
      if ( msg->status() & KMMsgStatusOld || msg->status() & KMMsgStatusRead )
        seenUids.append( msg->UID() );
      else
        unseenUids.append( msg->UID() );
      if (unget) unGetMsg(*it);
    }
    if ( !seenUids.isEmpty() ) {
      QStringList sets = KMFolderImap::makeSets( seenUids, true );
      for( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it ) {
        QString imappath = imapPath() + ";UID=" + ( *it );
        account()->setImapSeenStatus( folder(), imappath, true );
      }
    }
    if ( !unseenUids.isEmpty() ) {
      QStringList sets = KMFolderImap::makeSets( unseenUids, true );
      for( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it ) {
        QString imappath = imapPath() + ";UID=" + ( *it );
        account()->setImapSeenStatus( folder(), imappath, false );
      }
    }
    return;
  }

  QMap< QString, QStringList > groups;
  for ( QValueList<int>::Iterator it = ids.begin(); it != ids.end(); ++it ) {
    KMMessage *msg = 0;
    bool unget = !isMessage(*it);
    msg = getMsg(*it);
    if (!msg) continue;
    QString flags = statusToFlags(msg->status(), mPermanentFlags);
    // Collect uids for each type of flags.
    groups[flags].append(QString::number(msg->UID()));
    if (unget) unGetMsg(*it);
  }
  QMapIterator< QString, QStringList > dit;
  for ( dit = groups.begin(); dit != groups.end(); ++dit ) {
     QCString flags = dit.key().latin1();
     QStringList sets = makeSets( (*dit), true );
     // Send off a status setting job for each set.
     for (  QStringList::Iterator slit = sets.begin(); slit != sets.end(); ++slit ) {
       QString imappath = imapPath() + ";UID=" + ( *slit );
       account()->setImapStatus(folder(), imappath, flags);
     }
  }
  if ( mContentState == imapListingInProgress ) {
    // we're currently get'ing this folder
    // to make sure that we get the latest flags abort the current listing and
    // create a new one
    kdDebug(5006) << "Set status during folder listing, restarting listing." << endl;
    disconnect(this, SLOT(slotListFolderResult(KIO::Job *)));
    quiet( false );
    reallyGetFolder( QString::null );
  }
}

//-----------------------------------------------------------------------------
QStringList KMFolderImap::makeSets(const QStringList& uids, bool sort)
{
  QValueList<ulong> tmp;
  for ( QStringList::ConstIterator it = uids.begin(); it != uids.end(); ++it )
    tmp.append( (*it).toInt() );
  return makeSets(tmp, sort);
}

QStringList KMFolderImap::makeSets( QValueList<ulong>& uids, bool sort )
{
  QStringList sets;
  QString set;

  if (uids.size() == 1)
  {
    sets.append(QString::number(uids.first()));
    return sets;
  }

  if (sort) qHeapSort(uids);

  ulong last = 0;
  // needed to make a uid like 124 instead of 124:124
  bool inserted = false;
  /* iterate over uids and build sets like 120:122,124,126:150 */
  for ( QValueList<ulong>::Iterator it = uids.begin(); it != uids.end(); ++it )
  {
    if (it == uids.begin() || set.isEmpty()) {
      set = QString::number(*it);
      inserted = true;
    } else
    {
      if (last+1 != *it)
      {
        // end this range
        if (inserted)
          set += ',' + QString::number(*it);
        else
          set += ':' + QString::number(last) + ',' + QString::number(*it);
        inserted = true;
        if (set.length() > 100)
        {
          // just in case the server has a problem with longer lines..
          sets.append(set);
          set = "";
        }
      } else {
        inserted = false;
      }
    }
    last = *it;
  }
  // last element
  if (!inserted)
    set += ':' + QString::number(uids.last());

  if (!set.isEmpty()) sets.append(set);

  return sets;
}

//-----------------------------------------------------------------------------
void KMFolderImap::getUids(QValueList<int>& ids, QValueList<ulong>& uids)
{
  KMMsgBase *msg = 0;
  // get the uids
  for ( QValueList<int>::Iterator it = ids.begin(); it != ids.end(); ++it )
  {
    msg = getMsgBase(*it);
    if (!msg) continue;
    uids.append(msg->UID());
  }
}

void KMFolderImap::getUids(const QPtrList<KMMessage>& msgList, QValueList<ulong>& uids)
{
  KMMessage *msg = 0;

  QPtrListIterator<KMMessage> it( msgList );
  while ( (msg = it.current()) != 0 ) {
    ++it;
    if ( msg->UID() > 0 ) {
      uids.append( msg->UID() );
    }
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::expungeFolder(KMFolderImap * aFolder, bool quiet)
{
  aFolder->setNeedsCompacting(false);
  KURL url = account()->getUrl();
  url.setPath(aFolder->imapPath() + ";UID=*");
  if ( account()->makeConnection() != ImapAccountBase::Connected )
    return;
  KIO::SimpleJob *job = KIO::file_delete(url, false);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  ImapAccountBase::jobData jd( url.url(), 0 );
  jd.quiet = quiet;
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          account(), SLOT(slotSimpleResult(KIO::Job *)));
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotProcessNewMail( int errorCode, const QString &errorMsg )
{
  Q_UNUSED( errorMsg );
  disconnect( account(), SIGNAL( connectionResult(int, const QString&) ),
              this, SLOT( slotProcessNewMail(int, const QString&) ) );
  if ( !errorCode && processNewMail( false ) )
    return;
  emit numUnreadMsgsChanged( folder() );
  emit newMailChecked( folder() );
}

//-----------------------------------------------------------------------------
bool KMFolderImap::processNewMail(bool)
{
   // a little safety
  if ( !account() ) {
    kdDebug(5006) << "KMFolderImap::processNewMail - account is null!" << endl;
    return false;
  }
  if ( imapPath().isEmpty() ) {
    kdDebug(5006) << "KMFolderImap::processNewMail - imapPath of " << name() << " is empty!" << endl;
    // remove it locally
    setAlreadyRemoved( true );
    kmkernel->imapFolderMgr()->remove( folder() );
    return false;
  }
  // check the connection
  if ( account()->makeConnection() == ImapAccountBase::Error ) {
    kdDebug(5006) << "KMFolderImap::processNewMail - got no connection!" << endl;
    return false;
  } else if ( account()->makeConnection() == ImapAccountBase::Connecting )
  {
    // wait
    kdDebug(5006) << "KMFolderImap::processNewMail - waiting for connection: " << label() << endl;
    connect( account(), SIGNAL( connectionResult(int, const QString&) ),
        this, SLOT( slotProcessNewMail(int, const QString&) ) );
    return true;
  }
  KURL url = account()->getUrl();
  url.setPath(imapPath() + ";SECTION=UNSEEN UIDNEXT UIDVALIDITY");

  mMailCheckProgressItem = ProgressManager::createProgressItem(
              "MailCheckAccount" + account()->name(),
              "MailCheck" + folder()->prettyURL(),
              QStyleSheet::escape( folder()->prettyURL() ),
              i18n("updating message counts"),
              false,
              account()->useSSL() || account()->useTLS() );

  KIO::SimpleJob *job = KIO::stat(url, false);
  KIO::Scheduler::assignJobToSlave(account()->slave(), job);
  ImapAccountBase::jobData jd(url.url(), folder() );
  jd.cancellable = true;
  account()->insertJob(job, jd);
  connect(job, SIGNAL(result(KIO::Job *)),
          SLOT(slotStatResult(KIO::Job *)));
  return true;
}


//-----------------------------------------------------------------------------
void KMFolderImap::slotStatResult(KIO::Job * job)
{
  // The new count first: the end of the progress (numUnreadMsgsChanged())
  // ends this folder's part of the mail check, which counts its new mail.
  ImapAccountBase::JobIterator it = account()->findJob(job);
  // (nothing new: the next UID is that of the last count)
  bool nothingNew = false;
  if ( it != account()->jobsEnd() && !job->error() ) {
    KIO::UDSEntry uds = static_cast<KIO::StatJob*>(job)->statResult();
    long unseen = -1;
    ulong uidNext = 0;
    QString uidValidity;
    for (KIO::UDSEntry::ConstIterator it = uds.begin(); it != uds.end(); it++)
    {
      if ((*it).m_uds == KIO::UDS_SIZE)
        unseen = (*it).m_long;
      else if ( (*it).m_uds == KIO::UDS_EXTRA ) {
        if ( (*it).m_str.startsWith( "X-UidNext: " ) )
          uidNext = (*it).m_str.mid( 11 ).toULong();
        else if ( (*it).m_str.startsWith( "X-UidValidity: " ) )
          uidValidity = (*it).m_str.mid( 15 );
      }
    }
    if (mReadOnly)
    {
      if ( uidNext > 0 ) {
        mGuessedUnreadMsgs = -1;
        mGuessedUnreadMsgs = countUnread() + (long)uidNext - (long)lastUid() - 1;
        if (mGuessedUnreadMsgs < 0) mGuessedUnreadMsgs = 0;
      }
    } else if ( unseen >= 0 ) {
      mGuessedUnreadMsgs = unseen;
    }
    nothingNew = uidNext > 0 && !uidValidity.isEmpty() && mNewMailUidNext > 0
                 && uidValidity == mNewMailUidValidity && uidNext <= mNewMailUidNext;
    // (for a count without a mark: the next UID, if the SELECT has none)
    mStatusUidNext = uidNext;
    mStatusUidValidity = uidValidity;
  }
  slotCompleteMailCheckProgress();
  it = account()->findJob(job);
  const bool known = it != account()->jobsEnd();
  if ( known )
    account()->removeJob(it);
  // the new mail: none, or the messages since the last count (a job
  // killed with the check, killAllJobs(): nothing); without a mark (of
  // this UID validity) the STATUS gives it, when it has both
  if ( !known )
    return;
  const bool markFromStatus = !job->error() && mStatusUidNext > 0 && !mStatusUidValidity.isEmpty()
                              && ( mNewMailUidNext == 0 || mStatusUidValidity != mNewMailUidValidity );
  if ( markFromStatus ) {
    setNewMailUidNext( mStatusUidNext, mStatusUidValidity );
    mStatusUidNext = 0;
    mStatusUidValidity = QString::null;
  }
  if ( nothingNew || markFromStatus || job->error() )
    emit newMailChecked( folder() );
  else
    checkNewMail();
  if ( known && job->error() )
    account()->handleJobError( job, i18n("Error while getting folder information.") );
}

//-----------------------------------------------------------------------------
void KMFolderImap::checkNewMail()
{
  if ( !account() || account()->makeConnection() != ImapAccountBase::Connected ) {
    emit newMailChecked( folder() );
    return;
  }
  // the messages from the mark on (with their Message-ID, to know the
  // ones we put here); without a mark only the mailbox's next UID
  // (without a mark: the last message, for its UID)
  const ulong from = mNewMailUidNext;
  KURL url = account()->getUrl();
  url.setPath( imapPath() + ";UID=" + ( from > 0 ? QString::number( from ) + ":*" : QString( "*:*" ) )
               + ";SECTION=ENVELOPE;INFO=STRICT" );
  KIO::SimpleJob *job = KIO::get( url, false, false );
  KIO::Scheduler::assignJobToSlave( account()->slave(), job );
  ImapAccountBase::jobData jd( url.url(), folder() );
  jd.cancellable = true;
  // (the mark and its UID validity this count starts from)
  jd.items << QString::number( from ) << mNewMailUidValidity
           << QString::number( mStatusUidNext ) << mStatusUidValidity;
  mStatusUidNext = 0;
  mStatusUidValidity = QString::null;
  account()->insertJob( job, jd );
  connect( job, SIGNAL( result(KIO::Job *) ),
           SLOT( slotNewMailFetchResult(KIO::Job *) ) );
  connect( job, SIGNAL( data(KIO::Job *, const QByteArray &) ),
           SLOT( slotSimpleData(KIO::Job *, const QByteArray &) ) );
}

// The Message-ID of a message as KMail puts it into a folder and as it
// comes back from the server: the part in angle brackets.
static QString messageIdKey( const QString & id )
{
  const int start = id.find( '<' );
  const int end = start >= 0 ? id.find( '>', start ) : -1;
  if ( end > start )
    return id.mid( start, end - start + 1 );
  return id.stripWhiteSpace();
}

//-----------------------------------------------------------------------------
void KMFolderImap::rememberOwnMessages( QPtrList<KMMessage> & msgList )
{
  for ( KMMessage * msg = msgList.first(); msg; msg = msgList.next() ) {
    const QString key = messageIdKey( msg->msgId() );
    if ( key.isEmpty() )
      continue;
    mOwnMessages[ key ] += 1;
    mOwnMessageOrder.append( key );
  }
  // (at most the last 1000: those of copies that failed go)
  while ( mOwnMessageOrder.count() > 1000 ) {
    const QString key = mOwnMessageOrder.first();
    mOwnMessageOrder.remove( mOwnMessageOrder.begin() );
    QMap<QString, int>::Iterator own = mOwnMessages.find( key );
    if ( own != mOwnMessages.end() && --own.data() <= 0 )
      mOwnMessages.remove( own );
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotNewMailFetchResult( KIO::Job * job )
{
  ImapAccountBase::JobIterator it = account()->findJob( job );
  if ( it == account()->jobsEnd() )
    return;   // (killed with the check, killAllJobs())
  // (a fetch refused without a mark: the header of the SELECT before it
  // may still give one)
  const bool refused = job->error() == KIO::ERR_COULD_NOT_READ;
  if ( (*it).items.count() == 4 && ( !job->error() || ( refused && (*it).items[0].toULong() == 0 ) ) ) {
    const ulong from = (*it).items[0].toULong();
    const QString fromValidity = (*it).items[1];
    const ulong statusUidNext = (*it).items[2].toULong();
    const QString statusValidity = (*it).items[3];
    // the digest: a header with the mailbox's UID validity and next UID
    // (that of this SELECT: "X-Selected: now"), then a part with
    // "X-UID", "X-Flags" (none: 0) and the envelope of each message.  (A
    // part can repeat the one before: kio_imap4 writes one for each
    // response during the fetch.)
    const QString data = QString::fromLatin1( (*it).data.data(), (*it).data.size() );
    const QStringList parts = QStringList::split( "--IMAPDIGEST", data, true );
    QString validity;
    ulong uidNext = 0;
    bool selectedNow = false;
    int exists = -1;
    if ( !parts.isEmpty() ) {
      const QStringList lines = QStringList::split( "\r\n", parts.first() );
      for ( QStringList::ConstIterator line = lines.begin(); line != lines.end(); ++line ) {
        if ( ( *line ).startsWith( "X-uidValidity: " ) )
          validity = ( *line ).mid( 15 ).stripWhiteSpace();
        else if ( ( *line ).startsWith( "X-UidNext: " ) )
          uidNext = ( *line ).mid( 11 ).toULong();
        else if ( *line == "X-Selected: now" )
          selectedNow = true;
        else if ( ( *line ).startsWith( "X-Count: " ) )
          exists = ( *line ).mid( 9 ).toInt();
      }
    }
    const bool counting = !refused && from > 0 && validity == fromValidity;
    ulong next = counting ? from : 0;
    // The messages, each once: a UID's parts merged (an unsolicited FETCH
    // can come before the envelope), the flags of the last one, the
    // Message-ID of the one that has it.
    QMap<ulong, int> flagsOf;
    QMap<ulong, QString> messageIdOf;
    // (and the part with its envelope, for the balloon)
    QMap<ulong, QString> envelopeOf;
    for ( QStringList::ConstIterator part = parts.begin(); part != parts.end(); ++part ) {
      if ( part == parts.begin() )
        continue;
      ulong uid = 0;
      int flags = 0;
      QString messageId;
      bool envelope = false;
      // (unfolded: a long Message-ID goes on the next line)
      QString text = *part;
      text.replace( "\r\n ", " " );
      text.replace( "\r\n\t", " " );
      const QStringList lines = QStringList::split( "\r\n", text );
      for ( QStringList::ConstIterator line = lines.begin(); line != lines.end(); ++line ) {
        if ( ( *line ).startsWith( "X-UID: " ) )
          uid = ( *line ).mid( 7 ).toULong();
        else if ( ( *line ).startsWith( "X-Flags: " ) )
          flags = ( *line ).mid( 9 ).toInt();
        else if ( ( *line ).lower().startsWith( "message-id:" ) )
          messageId = messageIdKey( ( *line ).mid( 11 ) );
        if ( ( *line ).lower().startsWith( "from:" ) || ( *line ).lower().startsWith( "subject:" ) )
          envelope = true;
      }
      if ( uid == 0 )
        continue;
      flagsOf[ uid ] = flags;
      if ( !messageId.isEmpty() )
        messageIdOf[ uid ] = messageId;
      if ( envelope ) {
        // (unfolded, kio_imap4 folds long lines with a bare LF too)
        QString headers = *part;
        headers.replace( "\r\n", "\n" );
        headers.replace( "\n ", " " );
        headers.replace( "\n\t", " " );
        envelopeOf[ uid ] = headers;
      }
      next = QMAX( next, uid + 1 );
    }
    int count = 0;
    QValueList<ulong> counted;
    for ( QMap<ulong, int>::ConstIterator m = flagsOf.begin(); counting && m != flagsOf.end(); ++m ) {
      const ulong uid = m.key();
      if ( uid < from )
        continue;   // ("n:*" has the last one at least)
      // one we put here (moved, copied, uploaded) is no new mail
      const QString messageId = messageIdOf[ uid ];
      QMap<QString, int>::Iterator own = mOwnMessages.find( messageId );
      if ( !messageId.isEmpty() && own != mOwnMessages.end() ) {
        if ( --own.data() <= 0 )
          mOwnMessages.remove( own );
        QStringList::Iterator order = mOwnMessageOrder.find( messageId );
        if ( order != mOwnMessageOrder.end() )
          mOwnMessageOrder.remove( order );
        continue;
      }
      if ( m.data() & ( 1 | 8 ) )
        continue;   // (\Seen, \Deleted)
      ++count;
      counted.append( uid );
    }
    // The messages themselves for the balloon, those listed here (one
    // pass from the end: the new ones are the last; the folder opened for
    // it if it is closed, as INBOX listed for the filters)
    // (a folder closed and not listed up to them: none here)
    QMap<ulong, Q_UINT32> serNumOf;
    // (only an index of this UID validity: a UID of another one is
    // another message)
    if ( !counted.isEmpty() && uidValidity() == validity
         && ( folder()->isOpened() || mLastUid >= counted.first() )
         && open( "newmail" ) == 0 ) {
      QValueList<ulong>::ConstIterator want = counted.fromLast();
      for ( int i = this->count() - 1; i >= 0 && want != counted.end(); --i ) {
        const KMMsgBase * msgBase = getMsgBase( i );
        if ( !msgBase )
          continue;
        while ( want != counted.end() && msgBase->UID() < *want )
          want = ( want == counted.begin() ) ? counted.end() : --want;
        if ( want != counted.end() && msgBase->UID() == *want ) {
          const Q_UINT32 serNum = KMMsgDict::instance()->getMsgSerNum( folder(), i );
          if ( serNum )
            serNumOf[ *want ] = serNum;
        }
      }
      close( "newmail" );
    }
    // (in the order of arrival; one not here by its UID and the envelope
    // of this fetch, one without an envelope only counted)
    for ( QValueList<ulong>::ConstIterator uid = counted.begin(); uid != counted.end(); ++uid ) {
      QMap<ulong, Q_UINT32>::ConstIterator sn = serNumOf.find( *uid );
      if ( sn != serNumOf.end() ) {
        kmkernel->acctMgr()->addNewMessage( account(), sn.data(), folder()->idString() );
        continue;
      }
      QMap<ulong, QString>::ConstIterator envelope = envelopeOf.find( *uid );
      if ( envelope == envelopeOf.end() )
        continue;
      // (its headers: the part from its first line on, decoded as
      // those of a message listed)
      QString text = envelope.data();
      while ( text.startsWith( "\n" ) )
        text.remove( 0, 1 );
      KMMessage msg;
      msg.fromString( QCString( text.latin1() ) );
      AccountManager::NewMessage message;
      message.folderId = folder()->idString();
      message.uid = *uid;
      message.uidValidity = validity;
      message.listing = mListingsStarted;
      message.from = msg.fromStrip();
      message.subject = msg.subject();
      if ( !msg.headerField( "Date" ).isEmpty() )
        message.date = msg.date();
      kmkernel->acctMgr()->addNewEnvelope( account(), message );
    }
    if ( selectedNow )
      next = QMAX( next, uidNext );
    if ( !counting ) {
      // A mark taken now, nothing counted: after the last message, not
      // below the mailbox's next UID (of this SELECT, of an older one, of
      // the STATUS before); an empty mailbox without one: from 1 on.
      next = QMAX( next, uidNext );
      if ( statusValidity == validity )
        next = QMAX( next, statusUidNext );
      // (the header is that of this mailbox, selected now or before for
      // this fetch: kio_imap4 keeps its count up to date)
      if ( next == 0 && exists == 0 && flagsOf.isEmpty() )
        next = 1;
    }
    mNewMailCount += count;
    if ( next > 0 )
      setNewMailUidNext( next, validity );
  }
  emit newMailChecked( folder() );
  it = account()->findJob( job );
  if ( it == account()->jobsEnd() ) return;
  account()->removeJob( it );
  // (a fetch the server refused keeps the mark: the next check counts
  // again; that is no error to tell)
  if ( job->error() && job->error() != KIO::ERR_COULD_NOT_READ )
    account()->handleJobError( job, i18n( "Error while getting folder information." ) );
}

//-----------------------------------------------------------------------------
void KMFolderImap::setNewMailUidNext( ulong uidNext, const QString & uidValidity )
{
  if ( uidValidity == mNewMailUidValidity && uidNext <= mNewMailUidNext )
    return;
  mNewMailUidNext = uidNext;
  mNewMailUidValidity = uidValidity;
  writeConfig();
}

//-----------------------------------------------------------------------------
int KMFolderImap::takeNewMailCount()
{
  const int count = mNewMailCount;
  mNewMailCount = 0;
  return count;
}

//-----------------------------------------------------------------------------
int KMFolderImap::create()
{
  readConfig();
  mUnreadMsgs = -1;
  return KMFolderMbox::create();
}

QValueList<ulong> KMFolderImap::splitSets(const QString uids)
{
  QValueList<ulong> uidlist;

  // ex: 1205,1204,1203,1202,1236:1238
  QString buffer = QString::null;
  int setstart = -1;
  // iterate over the uids
  for (uint i = 0; i < uids.length(); i++)
  {
    QChar chr = uids[i];
    if (chr == ',')
    {
      if (setstart > -1)
      {
        // a range (uid:uid) was before
        for (int j = setstart; j <= buffer.toInt(); j++)
        {
          uidlist.append(j);
        }
        setstart = -1;
      } else {
        // single uid
        uidlist.append(buffer.toInt());
      }
      buffer = "";
    } else if (chr == ':') {
      // remember the start of the range
      setstart = buffer.toInt();
      buffer = "";
    } else if (chr.category() == QChar::Number_DecimalDigit) {
      // digit
      buffer += chr;
    } else {
      // ignore
    }
  }
  // process the last data
  if (setstart > -1)
  {
    for (int j = setstart; j <= buffer.toInt(); j++)
    {
      uidlist.append(j);
    }
  } else {
    uidlist.append(buffer.toInt());
  }

  return uidlist;
}

//-----------------------------------------------------------------------------
int KMFolderImap::expungeContents()
{
  // nuke the local cache
  int rc = KMFolderMbox::expungeContents();

  // set the deleted flag for all messages in the folder
  KURL url = account()->getUrl();
  url.setPath( imapPath() + ";UID=1:*");
  if ( account()->makeConnection() == ImapAccountBase::Connected )
  {
    KIO::SimpleJob *job = KIO::file_delete(url, false);
    KIO::Scheduler::assignJobToSlave(account()->slave(), job);
    ImapAccountBase::jobData jd( url.url(), 0 );
    jd.quiet = true;
    account()->insertJob(job, jd);
    connect(job, SIGNAL(result(KIO::Job *)),
            account(), SLOT(slotSimpleResult(KIO::Job *)));
  }
  /* Is the below correct? If we are expunging (in the folder sense, not the imap sense),
     why delete but not (imap-)expunge? Since the folder is not active there is no concept
     of "leaving the folder", so the setting really has little to do with it. */
  // if ( autoExpunge() )
    expungeFolder(this, true);
  getFolder();

  return rc;
}

//-----------------------------------------------------------------------------
void
KMFolderImap::setUserRights( unsigned int userRights )
{
  mUserRights = userRights;
  kdDebug(5006) << imapPath() << " setUserRights: " << userRights << endl;
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotCompleteMailCheckProgress()
{
  if ( mMailCheckProgressItem ) {
    mMailCheckProgressItem->setComplete();
    mMailCheckProgressItem = 0;
    emit numUnreadMsgsChanged( folder() );
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::setSubfolderState( imapState state )
{
  mSubfolderState = state;
  if ( state == imapNoInformation && folder()->child() )
  {
    // pass through to children
    KMFolderNode* node;
    QPtrListIterator<KMFolderNode> it( *folder()->child() );
    for ( ; (node = it.current()); )
    {
      ++it;
      if (node->isDir()) continue;
      KMFolder *folder = static_cast<KMFolder*>(node);
      static_cast<KMFolderImap*>(folder->storage())->setSubfolderState( state );
    }
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::setIncludeInMailCheck( bool check )
{
  bool changed = ( mCheckMail != check );
  mCheckMail = check;
  if ( changed )
    account()->slotUpdateFolderList();
}

//-----------------------------------------------------------------------------
void KMFolderImap::setAlreadyRemoved( bool removed )
{
  mAlreadyRemoved = removed;
  if ( folder()->child() )
  {
    // pass through to childs
    KMFolderNode* node;
    QPtrListIterator<KMFolderNode> it( *folder()->child() );
    for ( ; (node = it.current()); )
    {
      ++it;
      if (node->isDir()) continue;
      KMFolder *folder = static_cast<KMFolder*>(node);
      static_cast<KMFolderImap*>(folder->storage())->setAlreadyRemoved( removed );
    }
  }
}

void KMFolderImap::slotCreatePendingFolders( int errorCode, const QString& errorMsg )
{
  Q_UNUSED( errorMsg );
  disconnect( account(), SIGNAL( connectionResult( int, const QString& ) ),
              this, SLOT( slotCreatePendingFolders( int, const QString& ) ) );
  if ( !errorCode ) {
    QStringList::Iterator it = mFoldersPendingCreation.begin();
    for ( ; it != mFoldersPendingCreation.end(); ++it ) {
      createFolder( *it );
    }
  }
  mFoldersPendingCreation.clear();
}

//-----------------------------------------------------------------------------
void KMFolderImap::search( const KMSearchPattern* pattern )
{
  if ( !pattern || pattern->isEmpty() )
  {
    // not much to do here
    QValueList<Q_UINT32> serNums;
    emit searchResult( folder(), serNums, pattern, true );
    return;
  }
  SearchJob* job = new SearchJob( this, account(), pattern );
  connect( job, SIGNAL( searchDone( QValueList<Q_UINT32>, const KMSearchPattern*, bool ) ),
           this, SLOT( slotSearchDone( QValueList<Q_UINT32>, const KMSearchPattern*, bool ) ) );
  job->start();
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotSearchDone( QValueList<Q_UINT32> serNums,
                                   const KMSearchPattern* pattern,
                                   bool complete )
{
  emit searchResult( folder(), serNums, pattern, complete );
}

//-----------------------------------------------------------------------------
void KMFolderImap::search( const KMSearchPattern* pattern, Q_UINT32 serNum )
{
  if ( !pattern || pattern->isEmpty() )
  {
    // not much to do here
    emit searchDone( folder(), serNum, pattern, false );
    return;
  }
  SearchJob* job = new SearchJob( this, account(), pattern, serNum );
  connect( job, SIGNAL( searchDone( Q_UINT32, const KMSearchPattern*, bool ) ),
           this, SLOT( slotSearchDone( Q_UINT32, const KMSearchPattern*, bool ) ) );
  job->start();
}

//-----------------------------------------------------------------------------
void KMFolderImap::slotSearchDone( Q_UINT32 serNum, const KMSearchPattern* pattern,
                                   bool matches )
{
  emit searchDone( folder(), serNum, pattern, matches );
}

//-----------------------------------------------------------------------------
bool KMFolderImap::isMoveable() const
{
  return ( hasChildren() == HasNoChildren &&
      !folder()->isSystemFolder() ) ? true : false;
}

//-----------------------------------------------------------------------------
const ulong KMFolderImap::serNumForUID( ulong uid )
{
  if ( mUidMetaDataMap.find( uid ) ) {
    KMMsgMetaData *md = mUidMetaDataMap[uid];
    return md->serNum();
  } else {
    kdDebug(5006) << "serNumForUID: unknown uid " << uid << endl;
    return 0;
  }
}

//-----------------------------------------------------------------------------
void KMFolderImap::saveMsgMetaData( KMMessage* msg, ulong uid )
{
  if ( uid == 0 ) {
    uid = msg->UID();
  }
  ulong serNum = msg->getMsgSerNum();
  // QIntDict does not grow by itself, and with its 17 buckets a lookup in the
  // map of a big folder walks thousands of entries
  if ( mUidMetaDataMap.count() >= mUidMetaDataMap.size() )
    mUidMetaDataMap.resize( 2 * mUidMetaDataMap.size() + 1 );
  mUidMetaDataMap.replace( uid, new KMMsgMetaData(msg->status(), serNum) );
}

//-----------------------------------------------------------------------------
void KMFolderImap::setImapPath( const QString& path )
{
  if ( path.isEmpty() ) {
    kdWarning(5006) << k_funcinfo << "ignoring empty path" << endl;
  } else {
    mImapPath = path;
  }
}

void KMFolderImap::finishMailCheck( const char *dbg, imapState state )
{
  quiet( false );
  mContentState = state;
  if ( state == imapFinished && !mListingUidValidity.isEmpty() ) {
    // (a fetch refused, a message not taken: not as many as the
    // SELECT had, the listing proves nothing, nor does the next one
    // that starts from it)
    mIndexComplete = mListingFromComplete
                     && ( mListingExists < 0 || mListingExists == count() );
    if ( mIndexComplete ) {
      // (a bound of the same UID validity is not lowered)
      if ( mListedUidValidity == mListingUidValidity )
        mListedUidNext = QMAX( mListedUidNext, mListingUidNext );
      else
        mListedUidNext = mListingUidNext;
      mListedNumber = mListingsStarted;
      mListedUidValidity = mListingUidValidity;
    }
  }
  mListingUidValidity = QString::null;
  mListingUidNext = 0;
  mListingExists = -1;
  emit folderComplete( this, mContentState == imapFinished );
  close(dbg);
}

#include "kmfolderimap.moc"
