/* -*- mode: C++; c-file-style: "gnu" -*-
    This file is part of KMail, the KDE mail client.
    Copyright (c) 2000 Don Sanders <sanders@kde.org>

    KMail is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    KMail is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
// Virtual base class for mail folder with .*.index style index

#ifndef kmfolderindex_h
#define kmfolderindex_h

#include "folderstorage.h"
#include "kmmsglist.h"

class QTimer;

/**
 * @short A FolderStorage with an index for faster access to often used
 * message properties..
 *
 * This class represents a message store which has an index for providing fast
 * access to often used message properties, namely those displayed in the list
 * of messages (KMHeaders).
 * 
 * @author Don Sanders <sanders@kde.org>
 */

class KMFolderIndex: public FolderStorage
{
  Q_OBJECT
  //TODO:Have to get rid of this friend declaration and add necessary pure
  //virtuals to kmfolder.h so that KMMsgBase::parent() can be a plain KMFolder
  //rather than a KMFolderIndex. Need this for database indices.
  friend class ::KMMsgBase;
public:

  /** This enum indicates the status of the index file. It's returned by
      indexStatus().
   */
  enum IndexStatus { IndexOk,
                     IndexMissing,
                     IndexTooOld
  };

  /** Usually a parent is given. But in some cases there is no
    fitting parent object available. Then the name of the folder
    is used as the absolute path to the folder file. */
  KMFolderIndex(KMFolder* folder, const char* name=0);
  virtual ~KMFolderIndex();
  virtual int count(bool cache = false) const;

  virtual KMMsgBase* takeIndexEntry( int idx ) { return mMsgList.take( idx ); }
  virtual KMMsgInfo* setIndexEntry( int idx, KMMessage *msg );
  virtual void clearIndex(bool autoDelete=true, bool syncDict = false);
  virtual void truncateIndex();

  virtual const KMMsgBase* getMsgBase(int idx) const { return mMsgList[idx]; }
  virtual KMMsgBase* getMsgBase(int idx) { return mMsgList[idx]; }

  virtual int find(const KMMsgBase* msg) const { return mMsgList.find((KMMsgBase*)msg); }
  int find( const KMMessage * msg ) const { return FolderStorage::find( msg ); }

  /** Registered unique serial number for the index file */
  int serialIndexId() const { return mIndexId; }

  uchar *indexStreamBasePtr() { return mIndexStreamPtr; }

  bool indexSwapByteOrder() { return mIndexSwapByteOrder; }
  int  indexSizeOfLong() { return mIndexSizeOfLong; }

  virtual QString indexLocation() const;
  virtual int writeIndex( bool createEmptyIndex = false );

  void recreateIndex();

  /** Recreate the index from the folder contents once control returns to
      the event loop.  Used when a damaged index entry is found while
      reading it: recreating the index right away would delete the message
      infos that are in use. */
  void scheduleRecreateIndex();

  /** Record in the index file that it is about to become inconsistent with
      the folder: called before an index entry is appended or rewritten in
      place and before the folder contents change in a way that the index
      file does not reflect yet.  The flag is cleared by updateIndex() and
      writeIndex() once the index on disk is complete and synced; an index
      that is still marked dirty when the folder is opened (KMail crashed or
      the power failed) is not trusted.  Unless @p scheduleSync is false,
      updateIndex() is called a few seconds later to clean it again. */
  void markIndexDirty(bool scheduleSync = true);

public slots:
  /** Incrementally update the index if possible else call writeIndex */
  virtual int updateIndex();

private slots:
  void slotRecreateIndex();
  void slotSyncIndex();

protected:
  bool readIndex();

  /** Like readIndex(), but with @p acceptDirty also reads an index that is
      marked dirty (see markIndexDirty()), and with @p acceptPrefix keeps
      the intact entries before the first damaged one instead of failing.
      The caller must then check the result against the folder contents. */
  bool readIndexFile(bool acceptDirty, bool acceptPrefix);

  /** Why the last readIndex()/readIndexFile() failed. */
  enum IndexReadResult {
    IndexReadOk,
    IndexReadBadHeader, // no usable header: recreate the index
    IndexReadHandled,   // the header code already recreated the index, or
                        // the user chose to keep an index of a newer KMail
    IndexReadDirty,     // marked dirty, entries not checked
    IndexReadBadEntry,  // a damaged entry
    IndexReadFailed     // anything else (the stream may have been closed)
  };
  IndexReadResult mIndexReadResult;

  /** Read index header. Called from within readIndex(). */
  bool readIndexHeader(int *gv=0);

  /** Create index file from messages file and fill the message-info list
      mMsgList. Returns 0 on success and an errno value (see fopen) on
      failure. */
  virtual int createIndexFromContents() = 0;

  bool updateIndexStreamPtr(bool just_close=FALSE);

  /** Clear the dirty mark set by markIndexDirty() after syncing the index
      file.  Returns false if that failed. */
  bool markIndexClean();

  /** Tests whether the contents of this folder is newer than the index.
      Should return IndexTooOld if the index is older than the contents.
      Should return IndexMissing if there is contents but no index.
      Should return IndexOk if the folder doesn't exist anymore "physically"
      or if the index is not older than the contents.
  */
  virtual IndexStatus indexStatus() = 0;

    /** Inserts messages into the message dictionary by iterating over the
     * message list. The messages will get new serial numbers. This is only
     * used on newly appeared folders, where there is no .ids file yet, or
     * when that has been invalidated. */
  virtual void fillMessageDict();

  /** table of contents file */
  FILE* mIndexStream;
  /** list of index entries or messages */
  KMMsgList mMsgList;

  /** offset of header of index file */
  off_t mHeaderOffset;

  uchar *mIndexStreamPtr;
  int mIndexStreamPtrLength, mIndexId;
  bool mRecreateIndexPending;
  /** offset of the clean/dirty state in the index header, 0 if the index
      has no such field (written by an older KMail) */
  off_t mIndexStateOffset;
  /** the index file is marked dirty on disk */
  bool mIndexMarkedDirty;
  QTimer *mIndexSyncTimer;
  bool mIndexSwapByteOrder; // Index file was written with swapped byte order
  int mIndexSizeOfLong; // Index file was written with longs of this size
};

#endif /*kmfolderindex_h*/
