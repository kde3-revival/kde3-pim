// -*- mode: C++; c-file-style: "gnu" -*-
// kmreaderwin.cpp
// Author: Markus Wuebben <markus.wuebben@kde.org>

// define this to copy all html that is written to the readerwindow to
// filehtmlwriter.out in the current working directory
//#define KMAIL_READER_HTML_DEBUG 1

#include <config.h>

#include "kmreaderwin.h"

#include "globalsettings.h"
#include "kmversion.h"
#include "kmmainwidget.h"
#include "kmreadermainwin.h"
#include <libkdepim/kfileio.h>
#include "kmfolderindex.h"
#include "kmcommands.h"
#include "kmmsgpartdlg.h"
#include "mailsourceviewer.h"
using KMail::MailSourceViewer;
#include "partNode.h"
#include "kmmsgdict.h"
#include "messagesender.h"
#include "kcursorsaver.h"
#include "kmfolder.h"
#include "vcardviewer.h"
using KMail::VCardViewer;
#include "objecttreeparser.h"
using KMail::ObjectTreeParser;
#include "partmetadata.h"
using KMail::PartMetaData;
#include "attachmentstrategy.h"
using KMail::AttachmentStrategy;
#include "headerstrategy.h"
using KMail::HeaderStrategy;
#include "headerstyle.h"
using KMail::HeaderStyle;
#include "khtmlparthtmlwriter.h"
using KMail::HtmlWriter;
using KMail::KHtmlPartHtmlWriter;
#include "htmlstatusbar.h"
using KMail::HtmlStatusBar;
#include "folderjob.h"
using KMail::FolderJob;
#include "csshelper.h"
using KMail::CSSHelper;
#include "isubject.h"
using KMail::ISubject;
#include "urlhandlermanager.h"
using KMail::URLHandlerManager;
#include "interfaces/observable.h"
#include "util.h"

#include "broadcaststatus.h"

#include <kmime_mdn.h>
using namespace KMime;
#ifdef KMAIL_READER_HTML_DEBUG
#include "filehtmlwriter.h"
using KMail::FileHtmlWriter;
#include "teehtmlwriter.h"
using KMail::TeeHtmlWriter;
#endif

#include <kasciistringtools.h>
#include <kstringhandler.h>

#include <mimelib/mimepp.h>
#include <mimelib/body.h>
#include <mimelib/utility.h>

#include <kleo/specialjob.h>
#include <kleo/cryptobackend.h>
#include <kleo/cryptobackendfactory.h>

// KABC includes
#include <kabc/addressee.h>
#include <kabc/vcardconverter.h>

// khtml headers
#include <khtml_part.h>
#include <kparts/browserextension.h>
#include <kparts/componentfactory.h>
#include "webkitparthtmlwriter.h"
#include <khtmlview.h> // So that we can get rid of the frames
#include <dom/html_element.h>
#include <dom/html_block.h>
#include <dom/html_document.h>
#include <dom/dom_string.h>


#include <kapplication.h>
// for the click on attachment stuff (dnaber):
#include <kuserprofile.h>
#include <kcharsets.h>
#include <kpopupmenu.h>
#include <kstandarddirs.h>  // Sven's : for access and getpid
#include <kcursor.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kglobalsettings.h>
#include <krun.h>
#include <ktempfile.h>
#include <kprocess.h>
#include <kdialog.h>
#include <kaction.h>
#include <kiconloader.h>
#include <kmdcodec.h>
#include <kasciistricmp.h>
#include <kurldrag.h>

#include <qclipboard.h>
#include <qfileinfo.h>
#include <qvariant.h>
#include <qhbox.h>
#include <qtextcodec.h>
#include <qpaintdevicemetrics.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qsplitter.h>
#include <qstyle.h>

// X headers...
#undef Never
#undef Always

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

class NewByteArray : public QByteArray
{
public:
    NewByteArray &appendNULL();
    NewByteArray &operator+=( const char * );
    NewByteArray &operator+=( const QByteArray & );
    NewByteArray &operator+=( const QCString & );
    QByteArray& qByteArray();
};

NewByteArray& NewByteArray::appendNULL()
{
    QByteArray::detach();
    uint len1 = size();
    if ( !QByteArray::resize( len1 + 1 ) )
        return *this;
    *(data() + len1) = '\0';
    return *this;
}
NewByteArray& NewByteArray::operator+=( const char * newData )
{
    if ( !newData )
        return *this;
    QByteArray::detach();
    uint len1 = size();
    uint len2 = qstrlen( newData );
    if ( !QByteArray::resize( len1 + len2 ) )
        return *this;
    memcpy( data() + len1, newData, len2 );
    return *this;
}
NewByteArray& NewByteArray::operator+=( const QByteArray & newData )
{
    if ( newData.isNull() )
        return *this;
    QByteArray::detach();
    uint len1 = size();
    uint len2 = newData.size();
    if ( !QByteArray::resize( len1 + len2 ) )
        return *this;
    memcpy( data() + len1, newData.data(), len2 );
    return *this;
}
NewByteArray& NewByteArray::operator+=( const QCString & newData )
{
    if ( newData.isEmpty() )
        return *this;
    QByteArray::detach();
    uint len1 = size();
    uint len2 = newData.length(); // forget about the trailing 0x00 !
    if ( !QByteArray::resize( len1 + len2 ) )
        return *this;
    memcpy( data() + len1, newData.data(), len2 );
    return *this;
}
QByteArray& NewByteArray::qByteArray()
{
    return *((QByteArray*)this);
}

// This function returns the complete data that were in this
// message parts - *after* all encryption has been removed that
// could be removed.
// - This is used to store the message in decrypted form.
void KMReaderWin::objectTreeToDecryptedMsg( partNode* node,
                                            NewByteArray& resultingData,
                                            KMMessage& theMessage,
                                            bool weAreReplacingTheRootNode,
                                            int recCount )
{
  kdDebug(5006) << QString("-------------------------------------------------" ) << endl;
  kdDebug(5006) << QString("KMReaderWin::objectTreeToDecryptedMsg( %1 )  START").arg( recCount ) << endl;
  if( node ) {
    partNode* curNode = node;
    partNode* dataNode = curNode;
    partNode * child = node->firstChild();
    bool bIsMultipart = false;

    switch( curNode->type() ){
      case DwMime::kTypeText: {
kdDebug(5006) << "* text *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypeHtml:
kdDebug(5006) << "html" << endl;
            break;
          case DwMime::kSubtypeXVCard:
kdDebug(5006) << "v-card" << endl;
            break;
          case DwMime::kSubtypeRichtext:
kdDebug(5006) << "rich text" << endl;
            break;
          case DwMime::kSubtypeEnriched:
kdDebug(5006) << "enriched " << endl;
            break;
          case DwMime::kSubtypePlain:
kdDebug(5006) << "plain " << endl;
            break;
          default:
kdDebug(5006) << "default " << endl;
            break;
          }
        }
        break;
      case DwMime::kTypeMultipart: {
kdDebug(5006) << "* multipart *" << endl;
          bIsMultipart = true;
          switch( curNode->subType() ){
          case DwMime::kSubtypeMixed:
kdDebug(5006) << "mixed" << endl;
            break;
          case DwMime::kSubtypeAlternative:
kdDebug(5006) << "alternative" << endl;
            break;
          case DwMime::kSubtypeDigest:
kdDebug(5006) << "digest" << endl;
            break;
          case DwMime::kSubtypeParallel:
kdDebug(5006) << "parallel" << endl;
            break;
          case DwMime::kSubtypeSigned:
kdDebug(5006) << "signed" << endl;
            break;
          case DwMime::kSubtypeEncrypted: {
kdDebug(5006) << "encrypted" << endl;
              if ( child ) {
                /*
                    ATTENTION: This code is to be replaced by the new 'auto-detect' feature. --------------------------------------
                */
                partNode* data =
                  child->findType( DwMime::kTypeApplication, DwMime::kSubtypeOctetStream, false, true );
                if ( !data )
                  data = child->findType( DwMime::kTypeApplication, DwMime::kSubtypePkcs7Mime, false, true );
                if ( data && data->firstChild() )
                  dataNode = data;
              }
            }
            break;
          default :
kdDebug(5006) << "(  unknown subtype  )" << endl;
            break;
          }
        }
        break;
      case DwMime::kTypeMessage: {
kdDebug(5006) << "* message *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypeRfc822: {
kdDebug(5006) << "RfC 822" << endl;
              if ( child )
                dataNode = child;
            }
            break;
          }
        }
        break;
      case DwMime::kTypeApplication: {
kdDebug(5006) << "* application *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypePostscript:
kdDebug(5006) << "postscript" << endl;
            break;
          case DwMime::kSubtypeOctetStream: {
kdDebug(5006) << "octet stream" << endl;
              if ( child )
                dataNode = child;
            }
            break;
          case DwMime::kSubtypePgpEncrypted:
kdDebug(5006) << "pgp encrypted" << endl;
            break;
          case DwMime::kSubtypePgpSignature:
kdDebug(5006) << "pgp signed" << endl;
            break;
          case DwMime::kSubtypePkcs7Mime: {
kdDebug(5006) << "pkcs7 mime" << endl;
              // note: subtype Pkcs7Mime can also be signed
              //       and we do NOT want to remove the signature!
              if ( child && curNode->encryptionState() != KMMsgNotEncrypted )
                dataNode = child;
            }
            break;
          }
        }
        break;
      case DwMime::kTypeImage: {
kdDebug(5006) << "* image *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypeJpeg:
kdDebug(5006) << "JPEG" << endl;
            break;
          case DwMime::kSubtypeGif:
kdDebug(5006) << "GIF" << endl;
            break;
          }
        }
        break;
      case DwMime::kTypeAudio: {
kdDebug(5006) << "* audio *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypeBasic:
kdDebug(5006) << "basic" << endl;
            break;
          }
        }
        break;
      case DwMime::kTypeVideo: {
kdDebug(5006) << "* video *" << endl;
          switch( curNode->subType() ){
          case DwMime::kSubtypeMpeg:
kdDebug(5006) << "mpeg" << endl;
            break;
          }
        }
        break;
      case DwMime::kTypeModel:
kdDebug(5006) << "* model *" << endl;
        break;
    }


    DwHeaders& rootHeaders( theMessage.headers() );
    DwBodyPart * part = dataNode->dwPart() ? dataNode->dwPart() : 0;
    DwHeaders * headers(
        (part && part->hasHeaders())
        ? &part->Headers()
        : (  (weAreReplacingTheRootNode || !dataNode->parentNode())
            ? &rootHeaders
            : 0 ) );
    if( dataNode == curNode ) {
kdDebug(5006) << "dataNode == curNode:  Save curNode without replacing it." << endl;

      // A) Store the headers of this part IF curNode is not the root node
      //    AND we are not replacing a node that already *has* replaced
      //    the root node in previous recursion steps of this function...
      if( headers ) {
        if( dataNode->parentNode() && !weAreReplacingTheRootNode ) {
kdDebug(5006) << "dataNode is NOT replacing the root node:  Store the headers." << endl;
          resultingData += headers->AsString().c_str();
        } else if( weAreReplacingTheRootNode && part && part->hasHeaders() ){
kdDebug(5006) << "dataNode replace the root node:  Do NOT store the headers but change" << endl;
kdDebug(5006) << "                                 the Message's headers accordingly." << endl;
kdDebug(5006) << "              old Content-Type = " << rootHeaders.ContentType().AsString().c_str() << endl;
kdDebug(5006) << "              new Content-Type = " << headers->ContentType(   ).AsString().c_str() << endl;
          rootHeaders.ContentType()             = headers->ContentType();
          theMessage.setContentTransferEncodingStr(
              headers->HasContentTransferEncoding()
            ? headers->ContentTransferEncoding().AsString().c_str()
            : "" );
          rootHeaders.ContentDescription() = headers->ContentDescription();
          rootHeaders.ContentDisposition() = headers->ContentDisposition();
          theMessage.setNeedsAssembly();
        }
      }

      // B) Store the body of this part.
      if( headers && bIsMultipart && dataNode->firstChild() )  {
kdDebug(5006) << "is valid Multipart, processing children:" << endl;
        QCString boundary = headers->ContentType().Boundary().c_str();
        curNode = dataNode->firstChild();
        // store children of multipart
        while( curNode ) {
kdDebug(5006) << "--boundary" << endl;
          if( resultingData.size() &&
              ( '\n' != resultingData.at( resultingData.size()-1 ) ) )
            resultingData += QCString( "\n" );
          resultingData += QCString( "\n" );
          resultingData += "--";
          resultingData += boundary;
          resultingData += "\n";
          // note: We are processing a harmless multipart that is *not*
          //       to be replaced by one of it's children, therefor
          //       we set their doStoreHeaders to true.
          objectTreeToDecryptedMsg( curNode,
                                    resultingData,
                                    theMessage,
                                    false,
                                    recCount + 1 );
          curNode = curNode->nextSibling();
        }
kdDebug(5006) << "--boundary--" << endl;
        resultingData += "\n--";
        resultingData += boundary;
        resultingData += "--\n\n";
kdDebug(5006) << "Multipart processing children - DONE" << endl;
      } else if( part ){
        // store simple part
kdDebug(5006) << "is Simple part or invalid Multipart, storing body data .. DONE" << endl;
        resultingData += part->Body().AsString().c_str();
      }
    } else {
kdDebug(5006) << "dataNode != curNode:  Replace curNode by dataNode." << endl;
      bool rootNodeReplaceFlag = weAreReplacingTheRootNode || !curNode->parentNode();
      if( rootNodeReplaceFlag ) {
kdDebug(5006) << "                      Root node will be replaced." << endl;
      } else {
kdDebug(5006) << "                      Root node will NOT be replaced." << endl;
      }
      // store special data to replace the current part
      // (e.g. decrypted data or embedded RfC 822 data)
      objectTreeToDecryptedMsg( dataNode,
                                resultingData,
                                theMessage,
                                rootNodeReplaceFlag,
                                recCount + 1 );
    }
  }
  kdDebug(5006) << QString("\nKMReaderWin::objectTreeToDecryptedMsg( %1 )  END").arg( recCount ) << endl;
}


/*
 ===========================================================================


        E N D    O F     T E M P O R A R Y     M I M E     C O D E


 ===========================================================================
*/











void KMReaderWin::createWidgets() {
  QVBoxLayout * vlay = new QVBoxLayout( this );
  mSplitter = new QSplitter( Qt::Vertical, this, "mSplitter" );
  vlay->addWidget( mSplitter );
  mMimePartTree = new KMMimePartTree( this, mSplitter, "mMimePartTree" );
  mBox = new QHBox( mSplitter, "mBox" );
  setStyleDependantFrameWidth();
  mBox->setFrameStyle( mMimePartTree->frameStyle() );
  mColorBar = new HtmlStatusBar( mBox, "mColorBar" );
  mViewer = new KHTMLPart( mBox, "mViewer" );
  // [Reader] HtmlEngine=webkit (the default): WPE WebKit (kde3-wpepart's
  // mail mode) shows the messages; KHTML if it is not there or chosen.
  mWebPart = 0;
  mWebWriter = 0;
  KConfigGroup reader( KMKernel::config(), "Reader" );
  if ( reader.readEntry( "HtmlEngine", "webkit" ) == "webkit" ) {
    int error = 0;
    mWebPart = KParts::ComponentFactory::createPartInstanceFromLibrary<KParts::ReadOnlyPart>(
      "libkwpepart", mBox, "mWebViewer", this, "mWebPart", QStringList( "mode=mail" ), &error );
    // an older part without the mail mode
    if ( mWebPart && !mWebPart->property( "mailLoadExternal" ).isValid() ) {
      delete mWebPart;
      mWebPart = 0;
    }
    if ( mWebPart ) {
      mViewer->widget()->hide();
      mBox->setStretchFactor( mWebPart->widget(), 1 );
    }
    else
      kdWarning( 5006 ) << "KMReaderWin: the WPE WebKit part (libkwpepart) with its mail mode is not available"
                        << " (error " << error << "): KHTML shows the messages" << endl;
  }
  mSplitter->setOpaqueResize( KGlobalSettings::opaqueResize() );
  mSplitter->setResizeMode( mMimePartTree, QSplitter::KeepSize );
}

const int KMReaderWin::delay = 150;

//-----------------------------------------------------------------------------
KMReaderWin::KMReaderWin(QWidget *aParent,
			 QWidget *mainWindow,
			 KActionCollection* actionCollection,
                         const char *aName,
                         int aFlags )
  : QWidget(aParent, aName, aFlags | Qt::WDestructiveClose),
    mAttachmentStrategy( 0 ),
    mHeaderStrategy( 0 ),
    mHeaderStyle( 0 ),
    mUpdateReaderWinTimer( 0, "mUpdateReaderWinTimer" ),
    mResizeTimer( 0, "mResizeTimer" ),
    mDelayedMarkTimer( 0, "mDelayedMarkTimer" ),
    mOldGlobalOverrideEncoding( "---" ), // init with dummy value
    mCSSHelper( 0 ),
    mRootNode( 0 ),
    mMainWindow( mainWindow ),
    mActionCollection( actionCollection ),
    mMailToComposeAction( 0 ),
    mMailToReplyAction( 0 ),
    mMailToForwardAction( 0 ),
    mAddAddrBookAction( 0 ),
    mOpenAddrBookAction( 0 ),
    mCopyAction( 0 ),
    mCopyURLAction( 0 ),
    mUrlOpenAction( 0 ),
    mUrlSaveAsAction( 0 ),
    mAddBookmarksAction( 0 ),
    mStartIMChatAction( 0 ),
    mSelectAllAction( 0 ),
    mSelectEncodingAction( 0 ),
    mToggleFixFontAction( 0 ),
    mHtmlWriter( 0 ),
    mSavedRelativePosition( 0 ),
    mDecrytMessageOverwrite( false ),
    mShowSignatureDetails( false ),
    mShowAttachmentQuicklist( true )
{
  mSplitterSizes << 180 << 100;
  mMimeTreeMode = 1;
  mMimeTreeAtBottom = true;
  mAutoDelete = false;
  mLastSerNum = 0;
  mWaitingForSerNum = 0;
  mMessage = 0;
  mEmbeddedBytes = 0;
  mLastStatus = KMMsgStatusUnknown;
  mMsgDisplay = true;
  mPrinting = false;
  mShowColorbar = false;
  mAtmUpdate = false;

  createWidgets();
  createActions( actionCollection );
  initHtmlWidget();
  readConfig();

  mHtmlOverride = false;
  mHtmlLoadExtOverride = false;

  mLevelQuote = GlobalSettings::self()->collapseQuoteLevelSpin() - 1;

  connect( &mUpdateReaderWinTimer, SIGNAL(timeout()),
  	   this, SLOT(updateReaderWin()) );
  connect( &mResizeTimer, SIGNAL(timeout()),
  	   this, SLOT(slotDelayedResize()) );
  connect( &mDelayedMarkTimer, SIGNAL(timeout()),
           this, SLOT(slotTouchMessage()) );

}

void KMReaderWin::createActions( KActionCollection * ac ) {
  if ( !ac )
      return;

  KRadioAction *raction = 0;

  // header style
  KActionMenu *headerMenu =
    new KActionMenu( i18n("View->", "&Headers"), ac, "view_headers" );
  headerMenu->setToolTip( i18n("Choose display style of message headers") );

  connect( headerMenu, SIGNAL(activated()),
           this, SLOT(slotCycleHeaderStyles()) );

  raction = new KRadioAction( i18n("View->headers->", "&Enterprise Headers"), 0,
                              this, SLOT(slotEnterpriseHeaders()),
                              ac, "view_headers_enterprise" );
  raction->setToolTip( i18n("Show the list of headers in Enterprise style") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert(raction);

  raction = new KRadioAction( i18n("View->headers->", "&Fancy Headers"), 0,
                              this, SLOT(slotFancyHeaders()),
                              ac, "view_headers_fancy" );
  raction->setToolTip( i18n("Show the list of headers in a fancy format") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert( raction );

  raction = new KRadioAction( i18n("View->headers->", "&Brief Headers"), 0,
                              this, SLOT(slotBriefHeaders()),
                              ac, "view_headers_brief" );
  raction->setToolTip( i18n("Show brief list of message headers") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert( raction );

  raction = new KRadioAction( i18n("View->headers->", "&Standard Headers"), 0,
                              this, SLOT(slotStandardHeaders()),
                              ac, "view_headers_standard" );
  raction->setToolTip( i18n("Show standard list of message headers") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert( raction );

  raction = new KRadioAction( i18n("View->headers->", "&Long Headers"), 0,
                              this, SLOT(slotLongHeaders()),
                              ac, "view_headers_long" );
  raction->setToolTip( i18n("Show long list of message headers") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert( raction );

  raction = new KRadioAction( i18n("View->headers->", "&All Headers"), 0,
                              this, SLOT(slotAllHeaders()),
                              ac, "view_headers_all" );
  raction->setToolTip( i18n("Show all message headers") );
  raction->setExclusiveGroup( "view_headers_group" );
  headerMenu->insert( raction );

  // attachment style
  KActionMenu *attachmentMenu =
    new KActionMenu( i18n("View->", "&Attachments"), ac, "view_attachments" );
  attachmentMenu->setToolTip( i18n("Choose display style of attachments") );
  connect( attachmentMenu, SIGNAL(activated()),
           this, SLOT(slotCycleAttachmentStrategy()) );

  raction = new KRadioAction( i18n("View->attachments->", "&As Icons"), 0,
                              this, SLOT(slotIconicAttachments()),
                              ac, "view_attachments_as_icons" );
  raction->setToolTip( i18n("Show all attachments as icons. Click to see them.") );
  raction->setExclusiveGroup( "view_attachments_group" );
  attachmentMenu->insert( raction );

  raction = new KRadioAction( i18n("View->attachments->", "&Smart"), 0,
                              this, SLOT(slotSmartAttachments()),
                              ac, "view_attachments_smart" );
  raction->setToolTip( i18n("Show attachments as suggested by sender.") );
  raction->setExclusiveGroup( "view_attachments_group" );
  attachmentMenu->insert( raction );

  raction = new KRadioAction( i18n("View->attachments->", "&Inline"), 0,
                              this, SLOT(slotInlineAttachments()),
                              ac, "view_attachments_inline" );
  raction->setToolTip( i18n("Show all attachments inline (if possible)") );
  raction->setExclusiveGroup( "view_attachments_group" );
  attachmentMenu->insert( raction );

  raction = new KRadioAction( i18n("View->attachments->", "&Hide"), 0,
                              this, SLOT(slotHideAttachments()),
                              ac, "view_attachments_hide" );
  raction->setToolTip( i18n("Do not show attachments in the message viewer") );
  raction->setExclusiveGroup( "view_attachments_group" );
  attachmentMenu->insert( raction );

  // Set Encoding submenu
  mSelectEncodingAction = new KSelectAction( i18n( "&Set Encoding" ), "charset", 0,
                                 this, SLOT( slotSetEncoding() ),
                                 ac, "encoding" );
  QStringList encodings = KMMsgBase::supportedEncodings( false );
  encodings.prepend( i18n( "Auto" ) );
  mSelectEncodingAction->setItems( encodings );
  mSelectEncodingAction->setCurrentItem( 0 );

  mMailToComposeAction = new KAction( i18n("New Message To..."), "mail_new",
                                      0, this, SLOT(slotMailtoCompose()), ac,
                                      "mailto_compose" );
  mMailToReplyAction = new KAction( i18n("Reply To..."), "mail_reply",
                                    0, this, SLOT(slotMailtoReply()), ac,
				    "mailto_reply" );
  mMailToForwardAction = new KAction( i18n("Forward To..."), "mail_forward",
                                      0, this, SLOT(slotMailtoForward()), ac,
                                      "mailto_forward" );
  mAddAddrBookAction = new KAction( i18n("Add to Address Book"),
				    0, this, SLOT(slotMailtoAddAddrBook()),
				    ac, "add_addr_book" );
  mOpenAddrBookAction = new KAction( i18n("Open in Address Book"),
                                     0, this, SLOT(slotMailtoOpenAddrBook()),
                                     ac, "openin_addr_book" );
  mCopyAction = KStdAction::copy( this, SLOT(slotCopySelectedText()), ac, "kmail_copy");
  mSelectAllAction = new KAction( i18n("Select All Text"), CTRL+SHIFT+Key_A, this,
                                  SLOT(selectAll()), ac, "mark_all_text" );
  mCopyURLAction = new KAction( i18n("Copy Link Address"), 0, this,
				SLOT(slotUrlCopy()), ac, "copy_url" );
  mUrlOpenAction = new KAction( i18n("Open URL"), 0, this,
                                SLOT(slotUrlOpen()), ac, "open_url" );
  mAddBookmarksAction = new KAction( i18n("Bookmark This Link"),
                                     "bookmark_add",
                                     0, this, SLOT(slotAddBookmarks()),
                                     ac, "add_bookmarks" );
  mUrlSaveAsAction = new KAction( i18n("Save Link As..."), 0, this,
                                  SLOT(slotUrlSave()), ac, "saveas_url" );

  mToggleFixFontAction = new KToggleAction( i18n("Use Fi&xed Font"),
                                            Key_X, this, SLOT(slotToggleFixedFont()),
                                            ac, "toggle_fixedfont" );

  mStartIMChatAction = new KAction( i18n("Chat &With..."), 0, this,
				    SLOT(slotIMChat()), ac, "start_im_chat" );
}

// little helper function
KRadioAction *KMReaderWin::actionForHeaderStyle( const HeaderStyle * style, const HeaderStrategy * strategy ) {
  if ( !mActionCollection )
    return 0;
  const char * actionName = 0;
  if ( style == HeaderStyle::enterprise() )
    actionName = "view_headers_enterprise";
  if ( style == HeaderStyle::fancy() )
    actionName = "view_headers_fancy";
  else if ( style == HeaderStyle::brief() )
    actionName = "view_headers_brief";
  else if ( style == HeaderStyle::plain() ) {
    if ( strategy == HeaderStrategy::standard() )
      actionName = "view_headers_standard";
    else if ( strategy == HeaderStrategy::rich() )
      actionName = "view_headers_long";
    else if ( strategy == HeaderStrategy::all() )
      actionName = "view_headers_all";
  }
  if ( actionName )
    return static_cast<KRadioAction*>(mActionCollection->action(actionName));
  else
    return 0;
}

KRadioAction *KMReaderWin::actionForAttachmentStrategy( const AttachmentStrategy * as ) {
  if ( !mActionCollection )
    return 0;
  const char * actionName = 0;
  if ( as == AttachmentStrategy::iconic() )
    actionName = "view_attachments_as_icons";
  else if ( as == AttachmentStrategy::smart() )
    actionName = "view_attachments_smart";
  else if ( as == AttachmentStrategy::inlined() )
    actionName = "view_attachments_inline";
  else if ( as == AttachmentStrategy::hidden() )
    actionName = "view_attachments_hide";

  if ( actionName )
    return static_cast<KRadioAction*>(mActionCollection->action(actionName));
  else
    return 0;
}

void KMReaderWin::slotEnterpriseHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::enterprise(),
                             HeaderStrategy::rich() );
}

void KMReaderWin::slotFancyHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::fancy(),
                             HeaderStrategy::rich() );
}

void KMReaderWin::slotBriefHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::brief(),
                             HeaderStrategy::brief() );
}

void KMReaderWin::slotStandardHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::plain(),
                             HeaderStrategy::standard());
}

void KMReaderWin::slotLongHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::plain(),
                             HeaderStrategy::rich() );
}

void KMReaderWin::slotAllHeaders() {
  setHeaderStyleAndStrategy( HeaderStyle::plain(),
                             HeaderStrategy::all() );
}

void KMReaderWin::slotLevelQuote( int l )
{
  kdDebug( 5006 ) << "Old Level: " << mLevelQuote << " New Level: " << l << endl;

  mLevelQuote = l;
  saveRelativePosition();
  update(true);
}

void KMReaderWin::slotCycleHeaderStyles() {
  const HeaderStrategy * strategy = headerStrategy();
  const HeaderStyle * style = headerStyle();

  const char * actionName = 0;
  if ( style == HeaderStyle::enterprise() ) {
    slotFancyHeaders();
    actionName = "view_headers_fancy";
  }
  if ( style == HeaderStyle::fancy() ) {
    slotBriefHeaders();
    actionName = "view_headers_brief";
  } else if ( style == HeaderStyle::brief() ) {
    slotStandardHeaders();
    actionName = "view_headers_standard";
  } else if ( style == HeaderStyle::plain() ) {
    if ( strategy == HeaderStrategy::standard() ) {
      slotLongHeaders();
      actionName = "view_headers_long";
    } else if ( strategy == HeaderStrategy::rich() ) {
      slotAllHeaders();
      actionName = "view_headers_all";
    } else if ( strategy == HeaderStrategy::all() ) {
      slotEnterpriseHeaders();
      actionName = "view_headers_enterprise";
    }
  }

  if ( actionName )
    static_cast<KRadioAction*>( mActionCollection->action( actionName ) )->setChecked( true );
}


void KMReaderWin::slotIconicAttachments() {
  setAttachmentStrategy( AttachmentStrategy::iconic() );
}

void KMReaderWin::slotSmartAttachments() {
  setAttachmentStrategy( AttachmentStrategy::smart() );
}

void KMReaderWin::slotInlineAttachments() {
  setAttachmentStrategy( AttachmentStrategy::inlined() );
}

void KMReaderWin::slotHideAttachments() {
  setAttachmentStrategy( AttachmentStrategy::hidden() );
}

void KMReaderWin::slotCycleAttachmentStrategy() {
  setAttachmentStrategy( attachmentStrategy()->next() );
  KRadioAction * action = actionForAttachmentStrategy( attachmentStrategy() );
  assert( action );
  action->setChecked( true );
}


//-----------------------------------------------------------------------------
KMReaderWin::~KMReaderWin()
{
  delete mHtmlWriter; mHtmlWriter = 0;
  delete mCSSHelper;
  if (mAutoDelete) delete message();
  delete mRootNode; mRootNode = 0;
  removeTempFiles();
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotMessageArrived( KMMessage *msg )
{
  if (msg && ((KMMsgBase*)msg)->isMessage()) {
    if ( msg->getMsgSerNum() == mWaitingForSerNum ) {
      setMsg( msg, true );
    } else {
      kdDebug( 5006 ) <<  "KMReaderWin::slotMessageArrived - ignoring update" << endl;
    }
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::update( KMail::Interface::Observable * observable )
{
  if ( !mAtmUpdate ) {
    // reparse the msg
    kdDebug(5006) << "KMReaderWin::update - message" << endl;
    if ( mWebWriter ) {
      // WebKit shows every document it gets: parts of a message that
      // arrive one after the other (IMAP load on demand) would each show
      // the message again.  One update for a burst of them: 200 ms after
      // the last one (the cover of a message switch stays up meanwhile).
      mUpdateReaderWinTimer.start( 200, true );
      return;
    }
    updateReaderWin();
    return;
  }

  if ( !mRootNode )
    return;

  KMMessage* msg = static_cast<KMMessage*>( observable );
  assert( msg != 0 );

  // find our partNode and update it
  if ( !msg->lastUpdatedPart() ) {
    kdDebug(5006) << "KMReaderWin::update - no updated part" << endl;
    return;
  }
  partNode* node = mRootNode->findNodeForDwPart( msg->lastUpdatedPart() );
  if ( !node ) {
    kdDebug(5006) << "KMReaderWin::update - can't find node for part" << endl;
    return;
  }
  node->setDwPart( msg->lastUpdatedPart() );

  // update the tmp file
  // we have to set it writeable temporarily
  ::chmod( QFile::encodeName( mAtmCurrentName ), S_IRWXU );
  QByteArray data = node->msgPart().bodyDecodedBinary();
  size_t size = data.size();
  if ( node->msgPart().type() == DwMime::kTypeText && size) {
    size = KMail::Util::crlf2lf( data.data(), size );
  }
  KPIM::kBytesToFile( data.data(), size, mAtmCurrentName, false, false, false );
  ::chmod( QFile::encodeName( mAtmCurrentName ), S_IRUSR );

  mAtmUpdate = false;
}

//-----------------------------------------------------------------------------
void KMReaderWin::removeTempFiles()
{
  for (QStringList::Iterator it = mTempFiles.begin(); it != mTempFiles.end();
    it++)
  {
    QFile::remove(*it);
  }
  mTempFiles.clear();
  for (QStringList::Iterator it = mTempDirs.begin(); it != mTempDirs.end();
    it++)
  {
    QDir(*it).rmdir(*it);
  }
  mTempDirs.clear();
}


//-----------------------------------------------------------------------------
bool KMReaderWin::event(QEvent *e)
{
  if (e->type() == QEvent::ApplicationPaletteChange)
  {
    delete mCSSHelper;
    mCSSHelper = new KMail::CSSHelper( 	QPaintDeviceMetrics( mViewer->view() ) );
    if (message())
      message()->readConfig();
    update( true ); // Force update
    return true;
  }
  return QWidget::event(e);
}


//-----------------------------------------------------------------------------
void KMReaderWin::readConfig(void)
{
  const KConfigGroup mdnGroup( KMKernel::config(), "MDN" );
  /*should be: const*/ KConfigGroup reader( KMKernel::config(), "Reader" );

  delete mCSSHelper;
  mCSSHelper = new KMail::CSSHelper( QPaintDeviceMetrics( mViewer->view() ) );

  mNoMDNsWhenEncrypted = mdnGroup.readBoolEntry( "not-send-when-encrypted", true );

  mUseFixedFont = reader.readBoolEntry( "useFixedFont", false );
  if ( mToggleFixFontAction )
    mToggleFixFontAction->setChecked( mUseFixedFont );

  mHtmlMail = reader.readBoolEntry( "htmlMail", false );
  mHtmlLoadExternal = reader.readBoolEntry( "htmlLoadExternal", false );

  setHeaderStyleAndStrategy( HeaderStyle::create( reader.readEntry( "header-style", "fancy" ) ),
			     HeaderStrategy::create( reader.readEntry( "header-set-displayed", "rich" ) ) );
  KRadioAction *raction = actionForHeaderStyle( headerStyle(), headerStrategy() );
  if ( raction )
    raction->setChecked( true );

  setAttachmentStrategy( AttachmentStrategy::create( reader.readEntry( "attachment-strategy", "smart" ) ) );
  raction = actionForAttachmentStrategy( attachmentStrategy() );
  if ( raction )
    raction->setChecked( true );

  // if the user uses OpenPGP then the color bar defaults to enabled
  // else it defaults to disabled
  mShowColorbar = reader.readBoolEntry( "showColorbar", Kpgp::Module::getKpgp()->usePGP() );
  // if the value defaults to enabled and KMail (with color bar) is used for
  // the first time the config dialog doesn't know this if we don't save the
  // value now
  reader.writeEntry( "showColorbar", mShowColorbar );

  mMimeTreeAtBottom = reader.readEntry( "MimeTreeLocation", "bottom" ) != "top";
  const QString s = reader.readEntry( "MimeTreeMode", "smart" );
  if ( s == "never" )
    mMimeTreeMode = 0;
  else if ( s == "always" )
    mMimeTreeMode = 2;
  else
    mMimeTreeMode = 1;

  const int mimeH = reader.readNumEntry( "MimePaneHeight", 100 );
  const int messageH = reader.readNumEntry( "MessagePaneHeight", 180 );
  mSplitterSizes.clear();
  if ( mMimeTreeAtBottom )
    mSplitterSizes << messageH << mimeH;
  else
    mSplitterSizes << mimeH << messageH;

  adjustLayout();

  readGlobalOverrideCodec();

  if (message())
    update();
  KMMessage::readConfig();
}


void KMReaderWin::adjustLayout() {
  if ( mMimeTreeAtBottom )
    mSplitter->moveToLast( mMimePartTree );
  else
    mSplitter->moveToFirst( mMimePartTree );
  mSplitter->setSizes( mSplitterSizes );

  if ( mMimeTreeMode == 2 && mMsgDisplay )
    mMimePartTree->show();
  else
    mMimePartTree->hide();

  if ( mShowColorbar && mMsgDisplay )
    mColorBar->show();
  else
    mColorBar->hide();
}


void KMReaderWin::saveSplitterSizes( KConfigBase & c ) const {
  if ( !mSplitter || !mMimePartTree )
    return;
  if ( mMimePartTree->isHidden() )
    return; // don't rely on QSplitter maintaining sizes for hidden widgets.

  c.writeEntry( "MimePaneHeight", mSplitter->sizes()[ mMimeTreeAtBottom ? 1 : 0 ] );
  c.writeEntry( "MessagePaneHeight", mSplitter->sizes()[ mMimeTreeAtBottom ? 0 : 1 ] );
}

//-----------------------------------------------------------------------------
void KMReaderWin::writeConfig( bool sync ) const {
  KConfigGroup reader( KMKernel::config(), "Reader" );

  reader.writeEntry( "useFixedFont", mUseFixedFont );
  if ( headerStyle() )
    reader.writeEntry( "header-style", headerStyle()->name() );
  if ( headerStrategy() )
    reader.writeEntry( "header-set-displayed", headerStrategy()->name() );
  if ( attachmentStrategy() )
    reader.writeEntry( "attachment-strategy", attachmentStrategy()->name() );

  saveSplitterSizes( reader );

  if ( sync )
    kmkernel->slotRequestConfigSync();
}

//-----------------------------------------------------------------------------
void KMReaderWin::initHtmlWidget(void)
{
  mViewer->widget()->setFocusPolicy(WheelFocus);
  // Let's better be paranoid and disable plugins (it defaults to enabled):
  mViewer->setPluginsEnabled(false);
  mViewer->setJScriptEnabled(false); // just make this explicit
  mViewer->setJavaEnabled(false);    // just make this explicit
  mViewer->setMetaRefreshEnabled(false);
  mViewer->setURLCursor(KCursor::handCursor());
  // Espen 2000-05-14: Getting rid of thick ugly frames
  mViewer->view()->setLineWidth(0);
  // register our own event filter for shift-click
  mViewer->view()->viewport()->installEventFilter( this );

  if ( !htmlWriter() && mWebPart ) {
    mWebWriter = new KMail::WebKitPartHtmlWriter( mWebPart, 0 );
    mHtmlWriter = mWebWriter;
    mWebPart->widget()->setFocusPolicy( WheelFocus );
    KParts::BrowserExtension * ext = KParts::BrowserExtension::childObject( mWebPart );
    if ( ext ) {
      connect( ext, SIGNAL(openURLRequest(const KURL &, const KParts::URLArgs &)),
               this, SLOT(slotUrlOpen(const KURL &)) );
      connect( ext, SIGNAL(createNewWindow(const KURL &, const KParts::URLArgs &)),
               this, SLOT(slotUrlOpen(const KURL &)) );
      connect( this, SIGNAL(webCopy()), ext, SLOT(copy()) );
    }
    connect( mWebPart, SIGNAL(onURL(const QString &)), this, SLOT(slotUrlOn(const QString &)) );
    connect( mWebPart, SIGNAL(popupMenu(const QString &, const QPoint &)),
             SLOT(slotUrlPopup(const QString &, const QPoint &)) );
    connect( this, SIGNAL(webScrollBy(int, int)), mWebPart, SLOT(scrollBy(int, int)) );
    connect( this, SIGNAL(webSelectAll()), mWebPart, SLOT(selectAll()) );
  }
  if ( !htmlWriter() )
#ifdef KMAIL_READER_HTML_DEBUG
    mHtmlWriter = new TeeHtmlWriter( new FileHtmlWriter( QString::null ),
				     new KHtmlPartHtmlWriter( mViewer, 0 ) );
#else
    mHtmlWriter = new KHtmlPartHtmlWriter( mViewer, 0 );
#endif

  connect(mViewer->browserExtension(),
          SIGNAL(openURLRequest(const KURL &, const KParts::URLArgs &)),this,
          SLOT(slotUrlOpen(const KURL &)));
  connect(mViewer->browserExtension(),
          SIGNAL(createNewWindow(const KURL &, const KParts::URLArgs &)),this,
          SLOT(slotUrlOpen(const KURL &)));
  connect(mViewer,SIGNAL(onURL(const QString &)),this,
          SLOT(slotUrlOn(const QString &)));
  connect(mViewer,SIGNAL(popupMenu(const QString &, const QPoint &)),
          SLOT(slotUrlPopup(const QString &, const QPoint &)));
  connect( kmkernel->imProxy(), SIGNAL( sigContactPresenceChanged( const QString & ) ),
          this, SLOT( contactStatusChanged( const QString & ) ) );
  connect( kmkernel->imProxy(), SIGNAL( sigPresenceInfoExpired() ),
          this, SLOT( updateReaderWin() ) );
}

void KMReaderWin::contactStatusChanged( const QString &uid)
{
//  kdDebug( 5006 ) << k_funcinfo << " got a presence change for " << uid << endl;
  // get the list of nodes for this contact from the htmlView
  DOM::NodeList presenceNodes = mViewer->htmlDocument()
    .getElementsByName( DOM::DOMString( QString::fromLatin1("presence-") + uid ) );
  for ( unsigned int i = 0; i < presenceNodes.length(); ++i ) {
    DOM::Node n =  presenceNodes.item( i );
    kdDebug( 5006 ) << "name is " << n.nodeName().string() << endl;
    kdDebug( 5006 ) << "value of content was " << n.firstChild().nodeValue().string() << endl;
    QString newPresence = kmkernel->imProxy()->presenceString( uid );
    if ( newPresence.isNull() ) // KHTML crashes if you setNodeValue( QString::null )
      newPresence = QString::fromLatin1( "ENOIMRUNNING" );
    n.firstChild().setNodeValue( newPresence );
//    kdDebug( 5006 ) << "value of content is now " << n.firstChild().nodeValue().string() << endl;
  }
//  kdDebug( 5006 ) << "and we updated the above presence nodes" << uid << endl;
}

void KMReaderWin::setAttachmentStrategy( const AttachmentStrategy * strategy ) {
  mAttachmentStrategy = strategy ? strategy : AttachmentStrategy::smart();
  update( true );
}

void KMReaderWin::setHeaderStyleAndStrategy( const HeaderStyle * style,
					     const HeaderStrategy * strategy ) {
  mHeaderStyle = style ? style : HeaderStyle::fancy();
  mHeaderStrategy = strategy ? strategy : HeaderStrategy::rich();
  update( true );
}

//-----------------------------------------------------------------------------
void KMReaderWin::setOverrideEncoding( const QString & encoding )
{
  if ( encoding == mOverrideEncoding )
    return;

  mOverrideEncoding = encoding;
  if ( mSelectEncodingAction ) {
    if ( encoding.isEmpty() ) {
      mSelectEncodingAction->setCurrentItem( 0 );
    }
    else {
      QStringList encodings = mSelectEncodingAction->items();
      uint i = 0;
      for ( QStringList::const_iterator it = encodings.begin(), end = encodings.end(); it != end; ++it, ++i ) {
        if ( KGlobal::charsets()->encodingForName( *it ) == encoding ) {
          mSelectEncodingAction->setCurrentItem( i );
          break;
        }
      }
      if ( i == encodings.size() ) {
        // the value of encoding is unknown => use Auto
        kdWarning(5006) << "Unknown override character encoding \"" << encoding
                        << "\". Using Auto instead." << endl;
        mSelectEncodingAction->setCurrentItem( 0 );
        mOverrideEncoding = QString::null;
      }
    }
  }
  update( true );
}


void KMReaderWin::setPrintFont( const QFont& font )
{

  mCSSHelper->setPrintFont( font );
}

//-----------------------------------------------------------------------------
const QTextCodec * KMReaderWin::overrideCodec() const
{
  kdDebug(5006) << k_funcinfo << " mOverrideEncoding == '" << mOverrideEncoding << "'" << endl;
  if ( mOverrideEncoding.isEmpty() || mOverrideEncoding == "Auto" ) // Auto
    return 0;
  else
    return KMMsgBase::codecForName( mOverrideEncoding.latin1() );
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotSetEncoding()
{
  if ( mSelectEncodingAction->currentItem() == 0 ) // Auto
    mOverrideEncoding = QString();
  else
    mOverrideEncoding = KGlobal::charsets()->encodingForName( mSelectEncodingAction->currentText() );
  update( true );
}

//-----------------------------------------------------------------------------
void KMReaderWin::readGlobalOverrideCodec()
{
  // if the global character encoding wasn't changed then there's nothing to do
  if ( GlobalSettings::self()->overrideCharacterEncoding() == mOldGlobalOverrideEncoding )
    return;

  setOverrideEncoding( GlobalSettings::self()->overrideCharacterEncoding() );
  mOldGlobalOverrideEncoding = GlobalSettings::self()->overrideCharacterEncoding();
}

//-----------------------------------------------------------------------------
void KMReaderWin::setMsg(KMMessage* aMsg, bool force)
{
  if (aMsg)
      kdDebug(5006) << "(" << aMsg->getMsgSerNum() << ", last " << mLastSerNum << ") " << aMsg->subject() << " "
        << aMsg->fromStrip() << ", readyToShow " << (aMsg->readyToShow()) << endl;

	//Reset the level quote if the msg has changed.
  if (aMsg && aMsg->getMsgSerNum() != mLastSerNum ){
    mLevelQuote = GlobalSettings::self()->collapseQuoteLevelSpin()-1;
  }
  if ( mPrinting )
    mLevelQuote = -1;

  bool complete = true;
  if ( aMsg &&
       !aMsg->readyToShow() &&
       (aMsg->getMsgSerNum() != mLastSerNum) &&
       !aMsg->isComplete() )
    complete = false;

  // If not forced and there is aMsg and aMsg is same as mMsg then return
  if (!force && aMsg && mLastSerNum != 0 && aMsg->getMsgSerNum() == mLastSerNum)
    return;

  // WebKit: another message (or none) at once clears the reader; the part
  // would keep the previous message until the next one is ready (or
  // downloaded, with IMAP load on demand).
  if ( mWebWriter && mMsgDisplay ) {
    const bool other = aMsg ? ( aMsg->getMsgSerNum() != mLastSerNum ||
                                ( mLastSerNum == 0 && aMsg != mMessage ) )
                            : ( mLastSerNum != 0 || mMessage );
    if ( other )
      mWebWriter->clearView( mCSSHelper->backgroundColor() );
  }
  // the images embedded into the previous message are not this one's
  if ( aMsg ? aMsg->getMsgSerNum() != mLastSerNum || mLastSerNum == 0 : true )
    forgetEmbeddedResources();

  // (de)register as observer
  if (aMsg && message())
    message()->detach( this );
  if (aMsg)
    aMsg->attach( this );
  mAtmUpdate = false;

  // connect to the updates if we have hancy headers

  mDelayedMarkTimer.stop();

  mMessage = 0;
  if ( !aMsg ) {
    mWaitingForSerNum = 0; // otherwise it has been set
    mLastSerNum = 0;
  } else {
    mLastSerNum = aMsg->getMsgSerNum();
    // Check if the serial number can be used to find the assoc KMMessage
    // If so, keep only the serial number (and not mMessage), to avoid a dangling mMessage
    // when going to another message in the mainwindow.
    // Otherwise, keep only mMessage, this is fine for standalone KMReaderMainWins since
    // we're working on a copy of the KMMessage, which we own.
    if (message() != aMsg) {
      mMessage = aMsg;
      mLastSerNum = 0;
    }
  }

  if (aMsg) {
    aMsg->setOverrideCodec( overrideCodec() );
    aMsg->setDecodeHTML( htmlMail() );
    mLastStatus = aMsg->status();
    // FIXME: workaround to disable DND for IMAP load-on-demand
    if ( !aMsg->isComplete() )
      mViewer->setDNDEnabled( false );
    else
      mViewer->setDNDEnabled( true );
  } else {
    mLastStatus = KMMsgStatusUnknown;
  }

  // only display the msg if it is complete
  // otherwise we'll get flickering with progressively loaded messages
  if ( complete )
  {
    // Avoid flicker, somewhat of a cludge
    if (force) {
      // stop the timer to avoid calling updateReaderWin twice
      mUpdateReaderWinTimer.stop();
      updateReaderWin();
    }
    else if (mUpdateReaderWinTimer.isActive())
      mUpdateReaderWinTimer.changeInterval( delay );
    else
      mUpdateReaderWinTimer.start( 0, true );
  }

  if ( aMsg && (aMsg->isUnread() || aMsg->isNew()) && GlobalSettings::self()->delayedMarkAsRead() ) {
    if ( GlobalSettings::self()->delayedMarkTime() != 0 )
      mDelayedMarkTimer.start( GlobalSettings::self()->delayedMarkTime() * 1000, true );
    else
      slotTouchMessage();
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::clearCache()
{
  mUpdateReaderWinTimer.stop();
  clear();
  mDelayedMarkTimer.stop();
  mLastSerNum = 0;
  mWaitingForSerNum = 0;
  mMessage = 0;
}

// enter items for the "Important changes" list here:
static const char * const kmailChanges[] = {
  ""
};
static const int numKMailChanges =
  sizeof kmailChanges / sizeof *kmailChanges;

// enter items for the "new features" list here, so the main body of
// the welcome page can be left untouched (probably much easier for
// the translators). Note that the <li>...</li> tags are added
// automatically below:
static const char * const kmailNewFeatures[] = {
  I18N_NOOP("Full namespace support for IMAP"),
  I18N_NOOP("Offline mode"),
  I18N_NOOP("Sieve script management and editing"),
  I18N_NOOP("Account specific filtering"),
  I18N_NOOP("Filtering of incoming mail for online IMAP accounts"),
  I18N_NOOP("Online IMAP folders can be used when filtering into folders"),
  I18N_NOOP("Automatically delete older mails on POP servers")
};
static const int numKMailNewFeatures =
  sizeof kmailNewFeatures / sizeof *kmailNewFeatures;


//-----------------------------------------------------------------------------
//static
QString KMReaderWin::newFeaturesMD5()
{
  QCString str;
  for ( int i = 0 ; i < numKMailChanges ; ++i )
    str += kmailChanges[i];
  for ( int i = 0 ; i < numKMailNewFeatures ; ++i )
    str += kmailNewFeatures[i];
  KMD5 md5( str );
  return md5.base64Digest();
}

//-----------------------------------------------------------------------------
// An image of KDE's about pages (a file next to their style sheets) as a
// data: URL: KMail's WebKit document takes data: images only.  A regular
// file (not a symbolic link) directly in the style sheet's directory
// @p dir (both resolved), PNG/JPEG/GIF, at most 256 KB; anything else is
// dropped (an image less).
static QString aboutImageDataUrl( const QString & dir, const QString & name )
{
  static const Q_ULONG limit = 256 * 1024;
  const QString path = dir + "/" + name;
  QFileInfo fi( path );
  const QString ext = fi.extension( false ).lower();
  const char * type = ext == "png" ? "image/png" : ( ext == "jpg" || ext == "jpeg" ) ? "image/jpeg"
                      : ext == "gif" ? "image/gif" : 0;
  if ( !type || fi.isSymLink() || !fi.isFile() )
    return QString::null;
  char * realFile = realpath( QFile::encodeName( path ).data(), 0 );
  char * realDir = realpath( QFile::encodeName( dir ).data(), 0 );
  const bool inDir = realFile && realDir
                     && QFileInfo( QFile::decodeName( realFile ) ).dirPath( true ) == QFile::decodeName( realDir );
  free( realFile );
  free( realDir );
  if ( !inDir )
    return QString::null;
  QFile f( path );
  if ( !f.open( IO_ReadOnly ) )
    return QString::null;
  // at most the limit: one byte more means too large (also a file that
  // grew since)
  // (The file is checked, then opened: a swap in between is not guarded
  // against; these are KDE's installed data directories.)
  QByteArray data( limit + 1 );
  const Q_LONG n = f.readBlock( data.data(), limit + 1 );
  if ( n < 0 || (Q_ULONG)n > limit || f.status() != IO_Ok )
    return QString::null;   // too large, or a failed read: no image
  data.resize( n );
  return QString::fromLatin1( "data:%1;base64," ).arg( type ) + QString::fromLatin1( KCodecs::base64Encode( data ) );
}

// The about page (main.html, XHTML) for KMail's WebKit document:
// - @import "file"; replaced by the style sheet, its url(image) by data:
//   URLs (relative to the style sheet; WebKit reads nothing but the
//   document);
// - <div .../> written as <div ...></div>: HTML has no self-closing
//   <div>; an HTML parser (WebKit's) leaves it open and the rest of the
//   page lands inside it (KHTML's parser closed it).
static QString aboutPageForWebKit( const QString & html, const QString & baseDir )
{
  QRegExp import( "@import\\s*\"([^\"]+)\"\\s*;" );
  QRegExp url( "url\\(\\s*([^)\"':/\\s]+)\\s*\\)" );
  QString result = html;
  for ( int pos = import.search( result ); pos >= 0; pos = import.search( result, pos ) ) {
    // not one inside a comment (main.html has one: "maybe @import ...")
    const int open = result.findRev( "/*", pos );
    const int close = open >= 0 ? result.find( "*/", open + 2 ) : -1;
    if ( open >= 0 && ( close < 0 || close > pos ) ) {
      pos += import.matchedLength();
      continue;
    }
    QString file = import.cap( 1 );
    if ( !file.startsWith( "/" ) )
      file = baseDir + "/" + file;
    // a missing style sheet only costs looks: no error dialog
    QString css = KPIM::kFileToString( file, true, false );
    QString dir = QFileInfo( file ).dirPath( true );
    for ( int u = url.search( css ); u >= 0; u = url.search( css, u ) ) {
      const QString data = aboutImageDataUrl( dir, url.cap( 1 ) );
      QString replacement = data.isEmpty() ? QString::fromLatin1( "none" ) : "url(" + data + ")";
      css.replace( u, url.matchedLength(), replacement );
      u += replacement.length();
    }
    result.replace( pos, import.matchedLength(), css );
    pos += css.length();
  }
  QRegExp selfClosing( "<(div|span)(\\s[^<>]*)?/>" );
  for ( int pos = selfClosing.search( result ); pos >= 0; pos = selfClosing.search( result, pos ) ) {
    const QString replacement = "<" + selfClosing.cap( 1 ) + selfClosing.cap( 2 ) + "></" + selfClosing.cap( 1 ) + ">";
    result.replace( pos, selfClosing.matchedLength(), replacement );
    pos += replacement.length();
  }
  return result;
}

//-----------------------------------------------------------------------------
void KMReaderWin::displaySplashPage( const QString &info )
{
  mMsgDisplay = false;
  adjustLayout();

  QString location = locate("data", "kmail/about/main.html");
  QString content = KPIM::kFileToString(location);
  content = content.arg( locate( "data", "libkdepim/about/kde_infopage.css" ) );
  if ( kapp->reverseLayout() )
    content = content.arg( "@import \"%1\";" ).arg( locate( "data", "libkdepim/about/kde_infopage_rtl.css" ) );
  else
    content = content.arg( "" );

  QString fontSize = QString::number( pointsToPixel( mCSSHelper->bodyFont().pointSize() ) );
  QString appTitle = i18n("KMail");
  QString catchPhrase = ""; //not enough space for a catch phrase at default window size i18n("Part of the Kontact Suite");
  QString quickDescription = i18n("The email client for the K Desktop Environment.");
  content = content.arg(fontSize).arg(appTitle).arg(catchPhrase).arg(quickDescription).arg(info);
  if ( mWebWriter ) {
    // WebKit reads nothing but the document and images: the style sheets
    // go into it.
    htmlWriter()->begin( QString::null );
    htmlWriter()->write( aboutPageForWebKit( content, QFileInfo( location ).dirPath( true ) ) );
    htmlWriter()->end();
    return;
  }

  mViewer->begin(KURL( location ));
  mViewer->write( content );
  mViewer->end();
}

void KMReaderWin::displayBusyPage()
{
  QString info =
    i18n( "<h2 style='margin-top: 0px;'>Retrieving Folder Contents</h2><p>Please wait . . .</p>&nbsp;" );

  displaySplashPage( info );
}

void KMReaderWin::displayOfflinePage()
{
  QString info =
    i18n( "<h2 style='margin-top: 0px;'>Offline</h2><p>KMail is currently in offline mode. "
        "Click <a href=\"kmail:goOnline\">here</a> to go online . . .</p>&nbsp;" );

  displaySplashPage( info );
}


//-----------------------------------------------------------------------------
void KMReaderWin::displayAboutPage()
{
  QString info =
    i18n("%1: KMail version; %2: help:// URL; %3: homepage URL; "
	 "%4: prior KMail version; %5: prior KDE version; "
	 "%6: generated list of new features; "
	 "%7: First-time user text (only shown on first start); "
         "%8: generated list of important changes; "
	 "--- end of comment ---",
	 "<h2 style='margin-top: 0px;'>Welcome to KMail %1</h2><p>KMail is the email client for the K "
	 "Desktop Environment. It is designed to be fully compatible with "
	 "Internet mailing standards including MIME, SMTP, POP3 and IMAP."
	 "</p>\n"
	 "<ul><li>KMail has many powerful features which are described in the "
	 "<a href=\"%2\">documentation</a></li>\n"
	 "<li>The <a href=\"%3\">KMail homepage</A> offers information about "
	 "new versions of KMail</li></ul>\n"
         "%8\n" // important changes
	 "<p>Some of the new features in this release of KMail include "
	 "(compared to KMail %4, which is part of KDE %5):</p>\n"
	 "<ul>\n%6</ul>\n"
	 "%7\n"
	 "<p>We hope that you will enjoy KMail.</p>\n"
	 "<p>Thank you,</p>\n"
	     "<p style='margin-bottom: 0px'>&nbsp; &nbsp; The KMail Team</p>")
    .arg(KMAIL_VERSION) // KMail version
    .arg("help:/kmail/index.html") // KMail help:// URL
    .arg("http://kontact.kde.org/kmail/") // KMail homepage URL
    .arg("1.8").arg("3.4"); // prior KMail and KDE version

  QString featureItems;
  for ( int i = 0 ; i < numKMailNewFeatures ; i++ )
    featureItems += i18n("<li>%1</li>\n").arg( i18n( kmailNewFeatures[i] ) );

  info = info.arg( featureItems );

  if( kmkernel->firstStart() ) {
    info = info.arg( i18n("<p>Please take a moment to fill in the KMail "
			  "configuration panel at Settings-&gt;Configure "
			  "KMail.\n"
			  "You need to create at least a default identity and "
			  "an incoming as well as outgoing mail account."
			  "</p>\n") );
  } else {
    info = info.arg( QString::null );
  }

  if ( ( numKMailChanges > 1 ) || ( numKMailChanges == 1 && strlen(kmailChanges[0]) > 0 ) ) {
    QString changesText =
      i18n("<p><span style='font-size:125%; font-weight:bold;'>"
           "Important changes</span> (compared to KMail %1):</p>\n")
      .arg("1.8");
    changesText += "<ul>\n";
    for ( int i = 0 ; i < numKMailChanges ; i++ )
      changesText += i18n("<li>%1</li>\n").arg( i18n( kmailChanges[i] ) );
    changesText += "</ul>\n";
    info = info.arg( changesText );
  }
  else
    info = info.arg(""); // remove the %8

  displaySplashPage( info );
}

void KMReaderWin::enableMsgDisplay() {
  mMsgDisplay = true;
  adjustLayout();
}


//-----------------------------------------------------------------------------

void KMReaderWin::updateReaderWin()
{
  if (!mMsgDisplay) return;

  mViewer->setOnlyLocalReferences(!htmlLoadExternal());
  if ( mWebPart )
    mWebPart->setProperty( "mailLoadExternal", QVariant( htmlLoadExternal(), 0 ) );

  htmlWriter()->reset();

  KMFolder* folder = 0;
  if (message(&folder))
  {
    if ( mShowColorbar )
      mColorBar->show();
    else
      mColorBar->hide();
    displayMessage();
  }
  else
  {
    if ( mWebWriter ) {
      // WebKit: the reader keeps its size (and the last message) until
      // the empty page is really shown; often the next message comes
      // first.  A narrower or taller reader would make WebKit lay out and
      // paint the old message again, twice.
      QTimer::singleShot( 450, this, SLOT(slotHideEmptyReaderParts()) );
    } else {
      mColorBar->hide();
      mMimePartTree->hide();
      mMimePartTree->clear();
    }
    htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
    htmlWriter()->write( mCSSHelper->htmlHead( isFixedFont() ) + "</body></html>" );
    if ( mWebWriter )
      mWebWriter->setBlank();
    htmlWriter()->end();
  }

  if (mSavedRelativePosition)
  {
    QScrollView * scrollview = static_cast<QScrollView *>(mViewer->widget());
    scrollview->setContentsPos( 0,
      qRound( scrollview->contentsHeight() * mSavedRelativePosition ) );
    mSavedRelativePosition = 0;
  }
}

//-----------------------------------------------------------------------------
int KMReaderWin::pointsToPixel(int pointSize) const
{
  const QPaintDeviceMetrics pdm(mViewer->view());

  return (pointSize * pdm.logicalDpiY() + 36) / 72;
}

//-----------------------------------------------------------------------------
void KMReaderWin::showHideMimeTree( bool isPlainTextTopLevel ) {
  if ( mMimeTreeMode == 2 ||
       ( mMimeTreeMode == 1 && !isPlainTextTopLevel ) )
    mMimePartTree->show();
  else {
    // don't rely on QSplitter maintaining sizes for hidden widgets:
    KConfigGroup reader( KMKernel::config(), "Reader" );
    saveSplitterSizes( reader );
    mMimePartTree->hide();
  }
}

void KMReaderWin::displayMessage() {
  KMMessage * msg = message();

  mMimePartTree->clear();
  showHideMimeTree( !msg || // treat no message as "text/plain"
		    ( msg->type() == DwMime::kTypeText
		      && msg->subtype() == DwMime::kSubtypePlain ) );

  if ( !msg )
    return;

  msg->setOverrideCodec( overrideCodec() );

  htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
  htmlWriter()->queue( mCSSHelper->htmlHead( isFixedFont() ) );

  if (!parent())
    setCaption(msg->subject());

  removeTempFiles();

  mColorBar->setNeutralMode();

  parseMsg(msg);

  if( mColorBar->isNeutral() )
    mColorBar->setNormalMode();

  htmlWriter()->queue("</body></html>");
  // WebKit: the attachment list goes into the document before it is shown.
  if ( mWebWriter )
    mWebWriter->setAttachmentList( attachmentListHtml() );
  htmlWriter()->flush();

  if ( !mWebWriter )
    QTimer::singleShot( 1, this, SLOT(injectAttachments()) );
}


//-----------------------------------------------------------------------------
void KMReaderWin::parseMsg(KMMessage* aMsg)
{
#ifndef NDEBUG
  kdDebug( 5006 )
    << "parseMsg(KMMessage* aMsg "
    << ( aMsg == message() ? "==" : "!=" )
    << " aMsg )" << endl;
#endif

  KMMessagePart msgPart;
  QCString subtype, contDisp;
  QByteArray str;

  assert(aMsg!=0);

  aMsg->setIsBeingParsed( true );

  if ( mRootNode && !mRootNode->processed() )
  {
    kdWarning() << "The root node is not yet processed! Danger!\n";
    return;
  } else
    delete mRootNode;
  mRootNode = partNode::fromMessage( aMsg );
  const QCString mainCntTypeStr = mRootNode->typeString() + '/' + mRootNode->subTypeString();

  QString cntDesc = aMsg->subject();
  if( cntDesc.isEmpty() )
    cntDesc = i18n("( body part )");
  KIO::filesize_t cntSize = aMsg->msgSize();
  QString cntEnc;
  if( aMsg->contentTransferEncodingStr().isEmpty() )
    cntEnc = "7bit";
  else
    cntEnc = aMsg->contentTransferEncodingStr();

  // fill the MIME part tree viewer
  mRootNode->fillMimePartTree( 0,
			       mMimePartTree,
			       cntDesc,
			       mainCntTypeStr,
			       cntEnc,
			       cntSize );

  partNode* vCardNode = mRootNode->findType( DwMime::kTypeText, DwMime::kSubtypeXVCard );
  bool hasVCard = false;
  if( vCardNode ) {
    // ### FIXME: We should only do this if the vCard belongs to the sender,
    // ### i.e. if the sender's email address is contained in the vCard.
    const QString vcard = vCardNode->msgPart().bodyToUnicode( overrideCodec() );
    KABC::VCardConverter t;
    if ( !t.parseVCards( vcard ).empty() ) {
      hasVCard = true;
      kdDebug(5006) << "FOUND A VALID VCARD" << endl;
      writeMessagePartToTempFile( &vCardNode->msgPart(), vCardNode->nodeId() );
    }
  }
  htmlWriter()->queue( writeMsgHeader(aMsg, hasVCard, true ) );

  // show message content
  forgetEmbeddedResources();
  ObjectTreeParser otp( this );
  otp.parseObjectTree( mRootNode );

  // store encrypted/signed status information in the KMMessage
  //  - this can only be done *after* calling parseObjectTree()
  KMMsgEncryptionState encryptionState = mRootNode->overallEncryptionState();
  KMMsgSignatureState  signatureState  = mRootNode->overallSignatureState();
  aMsg->setEncryptionState( encryptionState );
  // Don't reset the signature state to "not signed" (e.g. if one canceled the
  // decryption of a signed messages which has already been decrypted before).
  if ( signatureState != KMMsgNotSigned ||
       aMsg->signatureState() == KMMsgSignatureStateUnknown ) {
    aMsg->setSignatureState( signatureState );
  }

  bool emitReplaceMsgByUnencryptedVersion = false;
  const KConfigGroup reader( KMKernel::config(), "Reader" );
  if ( reader.readBoolEntry( "store-displayed-messages-unencrypted", false ) ) {

  // Hack to make sure the S/MIME CryptPlugs follows the strict requirement
  // of german government:
  // --> All received encrypted messages *must* be stored in unencrypted form
  //     after they have been decrypted once the user has read them.
  //     ( "Aufhebung der Verschluesselung nach dem Lesen" )
  //
  // note: Since there is no configuration option for this, we do that for
  //       all kinds of encryption now - *not* just for S/MIME.
  //       This could be changed in the objectTreeToDecryptedMsg() function
  //       by deciding when (or when not, resp.) to set the 'dataNode' to
  //       something different than 'curNode'.


kdDebug(5006) << "\n\n\nKMReaderWin::parseMsg()  -  special post-encryption handling:\n1." << endl;
kdDebug(5006) << "(aMsg == msg) = "                               << (aMsg == message()) << endl;
kdDebug(5006) << "   (KMMsgStatusUnknown == mLastStatus) = "           << (KMMsgStatusUnknown == mLastStatus) << endl;
kdDebug(5006) << "|| (KMMsgStatusNew     == mLastStatus) = "           << (KMMsgStatusNew     == mLastStatus) << endl;
kdDebug(5006) << "|| (KMMsgStatusUnread  == mLastStatus) = "           << (KMMsgStatusUnread  == mLastStatus) << endl;
kdDebug(5006) << "(mIdOfLastViewedMessage != aMsg->msgId()) = "    << (mIdOfLastViewedMessage != aMsg->msgId()) << endl;
kdDebug(5006) << "   (KMMsgFullyEncrypted == encryptionState) = "     << (KMMsgFullyEncrypted == encryptionState) << endl;
kdDebug(5006) << "|| (KMMsgPartiallyEncrypted == encryptionState) = " << (KMMsgPartiallyEncrypted == encryptionState) << endl;
         // only proceed if we were called the normal way - not by
         // double click on the message (==not running in a separate window)
  if(    (aMsg == message())
         // only proceed if this message was not saved encryptedly before
         // to make sure only *new* messages are saved in decrypted form
      && (    (KMMsgStatusUnknown == mLastStatus)
           || (KMMsgStatusNew     == mLastStatus)
           || (KMMsgStatusUnread  == mLastStatus) )
         // avoid endless recursions
      && (mIdOfLastViewedMessage != aMsg->msgId())
         // only proceed if this message is (at least partially) encrypted
      && (    (KMMsgFullyEncrypted == encryptionState)
           || (KMMsgPartiallyEncrypted == encryptionState) ) ) {

kdDebug(5006) << "KMReaderWin  -  calling objectTreeToDecryptedMsg()" << endl;

    NewByteArray decryptedData;
    // note: The following call may change the message's headers.
    objectTreeToDecryptedMsg( mRootNode, decryptedData, *aMsg );
    // add a \0 to the data
    decryptedData.appendNULL();
    QCString resultString( decryptedData.data() );
kdDebug(5006) << "KMReaderWin  -  resulting data:" << resultString << endl;

    if( !resultString.isEmpty() ) {
kdDebug(5006) << "KMReaderWin  -  composing unencrypted message" << endl;
      // try this:
      aMsg->setBody( resultString );
      KMMessage* unencryptedMessage = new KMMessage( *aMsg );
      unencryptedMessage->setParent( 0 );
      // because this did not work:
      /*
      DwMessage dwMsg( aMsg->asDwString() );
      dwMsg.Body() = DwBody( DwString( resultString.data() ) );
      dwMsg.Body().Parse();
      KMMessage* unencryptedMessage = new KMMessage( &dwMsg );
      */
      //kdDebug(5006) << "KMReaderWin  -  resulting message:" << unencryptedMessage->asString() << endl;
      kdDebug(5006) << "KMReaderWin  -  attach unencrypted message to aMsg" << endl;
      aMsg->setUnencryptedMsg( unencryptedMessage );
      emitReplaceMsgByUnencryptedVersion = true;
    }
  }
  }

  // save current main Content-Type before deleting mRootNode
  const int rootNodeCntType = mRootNode ? mRootNode->type() : DwMime::kTypeText;
  const int rootNodeCntSubtype = mRootNode ? mRootNode->subType() : DwMime::kSubtypePlain;

  // store message id to avoid endless recursions
  setIdOfLastViewedMessage( aMsg->msgId() );

  if( emitReplaceMsgByUnencryptedVersion ) {
    kdDebug(5006) << "KMReaderWin  -  invoce saving in decrypted form:" << endl;
    emit replaceMsgByUnencryptedVersion();
  } else {
    kdDebug(5006) << "KMReaderWin  -  finished parsing and displaying of message." << endl;
    showHideMimeTree( rootNodeCntType == DwMime::kTypeText &&
		      rootNodeCntSubtype == DwMime::kSubtypePlain );
  }

  aMsg->setIsBeingParsed( false );
}


//-----------------------------------------------------------------------------
QString KMReaderWin::writeMsgHeader(KMMessage* aMsg, bool hasVCard, bool topLevel)
{
  kdFatal( !headerStyle(), 5006 )
    << "trying to writeMsgHeader() without a header style set!" << endl;
  kdFatal( !headerStrategy(), 5006 )
    << "trying to writeMsgHeader() without a header strategy set!" << endl;
  QString href;
  if (hasVCard)
    href = QString("file:") + KURL::encode_string( mTempFiles.last() );

  return headerStyle()->format( aMsg, headerStrategy(), href, mPrinting, topLevel );
}



//-----------------------------------------------------------------------------
void KMReaderWin::addEmbeddedResource( partNode * node, const QString & url )
{
  mEmbeddedResources[node] = url;
}

QString KMReaderWin::embeddedResourceURL( partNode * node ) const
{
  QMap<partNode *, QString>::ConstIterator it = mEmbeddedResources.find( node );
  return it == mEmbeddedResources.end() ? QString::null : it.data();
}

static const unsigned int embeddedBytesLimit = 64 * 1024 * 1024;

bool KMReaderWin::mayUseEmbeddedBytes( unsigned int bytes ) const
{
  return bytes <= embeddedBytesLimit && mEmbeddedBytes <= embeddedBytesLimit - bytes;
}

bool KMReaderWin::useEmbeddedBytes( unsigned int bytes )
{
  if ( !mayUseEmbeddedBytes( bytes ) )
    return false;
  mEmbeddedBytes += bytes;
  return true;
}

partNode * KMReaderWin::relatedRootOf( partNode * related )
{
  QMap<partNode *, partNode *>::ConstIterator it = mRelatedRoots.find( related );
  if ( it != mRelatedRoots.end() )
    return it.data();
  return mRelatedRoots[related] = related->relatedRoot();
}

const QMap<QString, partNode *> & KMReaderWin::embeddingScope( partNode * scope )
{
  QMap<partNode *, QMap<QString, partNode *> >::Iterator found = mEmbeddingScopes.find( scope );
  if ( found != mEmbeddingScopes.end() )
    return found.data();
  QMap<QString, partNode *> & parts = mEmbeddingScopes[scope];
  const partNode * root = relatedRootOf( scope );
  for ( partNode * child = scope->firstChild(); child; child = child->nextSibling() ) {
    const QString id = child->contentIdNoBrackets();
    if ( id.isEmpty() )
      continue;
    const bool candidate = child != root && child->type() != DwMime::kTypeMultipart
                           && child->type() != DwMime::kTypeMessage;
    // a Content-ID two children share is nobody's
    parts[id] = ( candidate && !parts.contains( id ) ) ? child : 0;
  }
  return parts;
}

void KMReaderWin::forgetEmbeddedResources()
{
  mEmbeddedResources.clear();
  mHiddenResources.clear();
  mEmbeddingScopes.clear();
  mRelatedRoots.clear();
  mFailedResources.clear();
  mEmbeddedBytes = 0;
}

//-----------------------------------------------------------------------------
QString KMReaderWin::writeMessagePartToTempFile( KMMessagePart* aMsgPart,
                                                 int aPartNum )
{
  QString fileName = aMsgPart->fileName();
  if( fileName.isEmpty() )
    fileName = aMsgPart->name();

  //--- Sven's save attachments to /tmp start ---
  QString fname = createTempDir( QString::number( aPartNum ) );
  if ( fname.isEmpty() )
    return QString();

  // strip off a leading path
  int slashPos = fileName.findRev( '/' );
  if( -1 != slashPos )
    fileName = fileName.mid( slashPos + 1 );
  if( fileName.isEmpty() )
    fileName = "unnamed";
  fname += "/" + fileName;

  QByteArray data = aMsgPart->bodyDecodedBinary();
  size_t size = data.size();
  if ( aMsgPart->type() == DwMime::kTypeText && size) {
    // convert CRLF to LF before writing text attachments to disk
    size = KMail::Util::crlf2lf( data.data(), size );
  }
  if( !KPIM::kBytesToFile( data.data(), size, fname, false, false, false ) )
    return QString::null;

  mTempFiles.append( fname );
  // make file read-only so that nobody gets the impression that he might
  // edit attached files (cf. bug #52813)
  ::chmod( QFile::encodeName( fname ), S_IRUSR );

  return fname;
}

QString KMReaderWin::createTempDir( const QString &param )
{
  KTempFile *tempFile = new KTempFile( QString::null, "." + param );
  tempFile->setAutoDelete( true );
  QString fname = tempFile->name();
  delete tempFile;

  if( ::access( QFile::encodeName( fname ), W_OK ) != 0 )
    // Not there or not writable
    if( ::mkdir( QFile::encodeName( fname ), 0 ) != 0
        || ::chmod( QFile::encodeName( fname ), S_IRWXU ) != 0 )
      return QString::null; //failed create

  assert( !fname.isNull() );

  mTempDirs.append( fname );
  return fname;
}

//-----------------------------------------------------------------------------
void KMReaderWin::showVCard( KMMessagePart * msgPart ) {
  const QString vCard = msgPart->bodyToUnicode( overrideCodec() );

  VCardViewer *vcv = new VCardViewer(this, vCard, "vCardDialog");
  vcv->show();
}

//-----------------------------------------------------------------------------
void KMReaderWin::printMsg()
{
  if (!message()) return;
  if ( mWebPart ) {
    // The part cannot print (yet): KHTML prints, as before the part.
    useKHTML();
    updateReaderWin();
  }
  mViewer->view()->print();
}


//-----------------------------------------------------------------------------
int KMReaderWin::msgPartFromUrl(const KURL &aUrl)
{
  if (aUrl.isEmpty()) return -1;

  bool ok;
  if ( aUrl.url().startsWith( "#att" ) ) {
    int res = aUrl.url().mid( 4 ).toInt( &ok );
    if ( ok ) return res;
  }

  if (!aUrl.isLocalFile()) return -1;

  QString path = aUrl.path();
  uint right = path.findRev('/');
  uint left = path.findRev('.', right);

  int res = path.mid(left + 1, right - left - 1).toInt(&ok);
  return (ok) ? res : -1;
}


//-----------------------------------------------------------------------------
void KMReaderWin::resizeEvent(QResizeEvent *)
{
  if( !mResizeTimer.isActive() )
  {
    //
    // Combine all resize operations that are requested as long a
    // the timer runs.
    //
    mResizeTimer.start( 100, true );
  }
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotDelayedResize()
{
  mSplitter->setGeometry(0, 0, width(), height());
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotTouchMessage()
{
  if ( !message() )
    return;

  if ( !message()->isNew() && !message()->isUnread() )
    return;

  SerNumList serNums;
  serNums.append( message()->getMsgSerNum() );
  KMCommand *command = new KMSetStatusCommand( KMMsgStatusRead, serNums );
  command->start();

  // should we send an MDN?
  if ( mNoMDNsWhenEncrypted &&
       message()->encryptionState() != KMMsgNotEncrypted &&
       message()->encryptionState() != KMMsgEncryptionStateUnknown )
    return;

  KMFolder *folder = message()->parent();
  if (folder &&
     (folder->isOutbox() || folder->isSent() || folder->isTrash() ||
      folder->isDrafts() || folder->isTemplates() ) )
    return;

  if ( KMMessage * receipt = message()->createMDN( MDN::ManualAction,
						   MDN::Displayed,
						   true /* allow GUI */ ) )
    if ( !kmkernel->msgSender()->send( receipt ) ) // send or queue
      KMessageBox::error( this, i18n("Could not send MDN.") );
}


//-----------------------------------------------------------------------------
void KMReaderWin::closeEvent(QCloseEvent *e)
{
  QWidget::closeEvent(e);
  writeConfig();
}


bool foundSMIMEData( const QString aUrl,
                     QString& displayName,
                     QString& libName,
                     QString& keyId )
{
  static QString showCertMan("showCertificate#");
  displayName = "";
  libName = "";
  keyId = "";
  int i1 = aUrl.find( showCertMan );
  if( -1 < i1 ) {
    i1 += showCertMan.length();
    int i2 = aUrl.find(" ### ", i1);
    if( i1 < i2 )
    {
      displayName = aUrl.mid( i1, i2-i1 );
      i1 = i2+5;
      i2 = aUrl.find(" ### ", i1);
      if( i1 < i2 )
      {
        libName = aUrl.mid( i1, i2-i1 );
        i2 += 5;

        keyId = aUrl.mid( i2 );
        /*
        int len = aUrl.length();
        if( len > i2+1 ) {
          keyId = aUrl.mid( i2, 2 );
          i2 += 2;
          while( len > i2+1 ) {
            keyId += ':';
            keyId += aUrl.mid( i2, 2 );
            i2 += 2;
          }
        }
        */
      }
    }
  }
  return !keyId.isEmpty();
}


//-----------------------------------------------------------------------------
// A link of the WebKit document: the URL of KMail's document it stands
// for (WebKitPartHtmlWriter::originalURL()); an unknown or expired one is
// an invalid URL, which nobody handles.
KURL KMReaderWin::originalURL( const KURL & url ) const
{
  if ( url.protocol() != "kmail-link" )
    return url;
  const QString original = mWebWriter ? mWebWriter->originalURL( url.url() ) : QString::null;
  return original.isEmpty() ? KURL() : KURL( original );
}

void KMReaderWin::slotUrlOn(const QString &aUrlIn)
{
  const KURL original = originalURL( KURL( aUrlIn ) );
  if ( !original.isValid() && KURL( aUrlIn ).protocol() == "kmail-link" ) {
    KPIM::BroadcastStatus::instance()->reset();
    return;
  }
  const QString aUrl = original.url();
  const KURL url(aUrl);
  if ( url.protocol() == "kmail" || url.protocol() == "x-kmail"
       || (url.protocol().isEmpty() && url.path().isEmpty()) ) {
    mViewer->setDNDEnabled( false );
  } else {
    mViewer->setDNDEnabled( true );
  }

  if ( aUrl.stripWhiteSpace().isEmpty() ) {
    KPIM::BroadcastStatus::instance()->reset();
    return;
  }

  mUrlClicked = url;

  const QString msg = URLHandlerManager::instance()->statusBarMessage( url, this );

  kdWarning( msg.isEmpty(), 5006 ) << "KMReaderWin::slotUrlOn(): Unhandled URL hover!" << endl;
  KPIM::BroadcastStatus::instance()->setTransientStatusMsg( msg );
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlOpen(const KURL &url, const KParts::URLArgs &)
{
  const KURL aUrl = originalURL( url );
  if ( url.protocol() == "kmail-link" && !aUrl.isValid() )
    return;   // a link of a document no longer shown, or none of ours
  mUrlClicked = aUrl;

  if ( URLHandlerManager::instance()->handleClick( aUrl, this ) )
    return;

  kdWarning( 5006 ) << "KMReaderWin::slotOpenUrl(): Unhandled URL click!" << endl;
  emit urlClicked( aUrl, Qt::LeftButton );
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlPopup(const QString &aUrl, const QPoint& aPos)
{
  const KURL url = originalURL( KURL( aUrl ) );
  if ( KURL( aUrl ).protocol() == "kmail-link" && !url.isValid() )
    return;
  mUrlClicked = url;

  if ( URLHandlerManager::instance()->handleContextMenuRequest( url, aPos, this ) )
    return;

  if ( message() ) {
    kdWarning( 5006 ) << "KMReaderWin::slotUrlPopup(): Unhandled URL right-click!" << endl;
    emit popupMenu( *message(), url, aPos );
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::showAttachmentPopup( int id, const QString & name, const QPoint & p )
{
  mAtmCurrent = id;
  mAtmCurrentName = name;
  KPopupMenu *menu = new KPopupMenu();
  menu->insertItem(SmallIcon("fileopen"),i18n("to open", "Open"), 1);
  menu->insertItem(i18n("Open With..."), 2);
  menu->insertItem(i18n("to view something", "View"), 3);
  menu->insertItem(SmallIcon("filesaveas"),i18n("Save As..."), 4);
  menu->insertItem(SmallIcon("editcopy"), i18n("Copy"), 9 );
  if ( GlobalSettings::self()->allowAttachmentEditing() )
    menu->insertItem(SmallIcon("edit"), i18n("Edit Attachment"), 8 );
  if ( GlobalSettings::self()->allowAttachmentDeletion() )
    menu->insertItem(SmallIcon("editdelete"), i18n("Delete Attachment"), 7 );
  if ( name.endsWith( ".xia", false ) &&
       Kleo::CryptoBackendFactory::instance()->protocol( "Chiasmus" ) )
    menu->insertItem( i18n( "Decrypt With Chiasmus..." ), 6 );
  menu->insertItem(i18n("Properties"), 5);
  connect(menu, SIGNAL(activated(int)), this, SLOT(slotHandleAttachment(int)));
  menu->exec( p ,0 );
  delete menu;
}

//-----------------------------------------------------------------------------
void KMReaderWin::setStyleDependantFrameWidth()
{
  if ( !mBox )
    return;
  // set the width of the frame to a reasonable value for the current GUI style
  int frameWidth;
  if( style().isA("KeramikStyle") )
    frameWidth = style().pixelMetric( QStyle::PM_DefaultFrameWidth ) - 1;
  else
    frameWidth = style().pixelMetric( QStyle::PM_DefaultFrameWidth );
  if ( frameWidth < 0 )
    frameWidth = 0;
  if ( frameWidth != mBox->lineWidth() )
    mBox->setLineWidth( frameWidth );
}

//-----------------------------------------------------------------------------
void KMReaderWin::styleChange( QStyle& oldStyle )
{
  setStyleDependantFrameWidth();
  QWidget::styleChange( oldStyle );
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotHandleAttachment( int choice )
{
  mAtmUpdate = true;
  partNode* node = mRootNode ? mRootNode->findId( mAtmCurrent ) : 0;
  if ( mAtmCurrentName.isEmpty() && node )
    mAtmCurrentName = tempFileUrlFromPartNode( node ).path();
  if ( choice < 7 ) {
  KMHandleAttachmentCommand* command = new KMHandleAttachmentCommand(
      node, message(), mAtmCurrent, mAtmCurrentName,
      KMHandleAttachmentCommand::AttachmentAction( choice ), 0, this );
  connect( command, SIGNAL( showAttachment( int, const QString& ) ),
      this, SLOT( slotAtmView( int, const QString& ) ) );
  command->start();
  } else if ( choice == 7 ) {
    slotDeleteAttachment( node );
  } else if ( choice == 8 ) {
    slotEditAttachment( node );
  } else if ( choice == 9 ) {
    if ( !node ) return;
    KURL::List urls;
    KURL url = tempFileUrlFromPartNode( node );
    if (!url.isValid() ) return;
    urls.append( url );
    KURLDrag* drag = new KURLDrag( urls, this );
    QApplication::clipboard()->setData( drag, QClipboard::Clipboard );
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotFind()
{
  if ( mWebPart ) {
    KAction * find = mWebPart->actionCollection()->action( "find" );
    if ( find )
      find->activate();
    return;
  }
  mViewer->findText();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotFindNext()
{
  if ( mWebPart ) {
    KAction * next = mWebPart->actionCollection()->action( "findNext" );
    if ( next )
      next->activate();
    return;
  }
  mViewer->findTextNext();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotToggleFixedFont()
{
  mUseFixedFont = !mUseFixedFont;
  saveRelativePosition();
  update(true);
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotCopySelectedText()
{
  if ( mWebPart ) {
    emit webCopy();
    return;
  }
  kapp->clipboard()->setText( mViewer->selectedText() );
}


//-----------------------------------------------------------------------------
void KMReaderWin::atmViewMsg(KMMessagePart* aMsgPart)
{
  assert(aMsgPart!=0);
  KMMessage* msg = new KMMessage;
  msg->fromString(aMsgPart->bodyDecoded());
  assert(msg != 0);
  msg->setMsgSerNum( 0 ); // because lookups will fail
  // some information that is needed for imap messages with LOD
  msg->setParent( message()->parent() );
  msg->setUID(message()->UID());
  msg->setReadyToShow(true);
  KMReaderMainWin *win = new KMReaderMainWin();
  win->showMsg( overrideEncoding(), msg );
  win->show();
}


void KMReaderWin::setMsgPart( partNode * node ) {
  htmlWriter()->reset();
  mColorBar->hide();
  htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
  htmlWriter()->write( mCSSHelper->htmlHead( isFixedFont() ) );
  // end ###
  if ( node ) {
    ObjectTreeParser otp( this, 0, true );
    otp.parseObjectTree( node );
  }
  // ### this, too
  htmlWriter()->queue( "</body></html>" );
  htmlWriter()->flush();
}

//-----------------------------------------------------------------------------
void KMReaderWin::setMsgPart( KMMessagePart* aMsgPart, bool aHTML,
			      const QString& aFileName, const QString& pname )
{
  KCursorSaver busy(KBusyPtr::busy());
  if (kasciistricmp(aMsgPart->typeStr(), "message")==0) {
      // if called from compose win
      KMMessage* msg = new KMMessage;
      assert(aMsgPart!=0);
      msg->fromString(aMsgPart->bodyDecoded());
      mMainWindow->setCaption(msg->subject());
      setMsg(msg, true);
      setAutoDelete(true);
  } else if (kasciistricmp(aMsgPart->typeStr(), "text")==0) {
      if (kasciistricmp(aMsgPart->subtypeStr(), "x-vcard") == 0) {
        showVCard( aMsgPart );
	return;
      }
      htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
      htmlWriter()->queue( mCSSHelper->htmlHead( isFixedFont() ) );

      if (aHTML && (kasciistricmp(aMsgPart->subtypeStr(), "html")==0)) { // HTML
        if ( htmlWriter()->isolatesHtml() )
          // WebKit: a document of its own, as the HTML parts of a message
          // (sanitized, no scripts, nothing remote unless allowed).
          htmlWriter()->queueIsolatedHtml( aMsgPart->bodyToUnicode( overrideCodec() ) );
        else
          // ### this is broken. It doesn't stip off the HTML header and footer!
          htmlWriter()->queue( aMsgPart->bodyToUnicode( overrideCodec() ) );
        mColorBar->setHtmlMode();
      } else { // plain text
        const QCString str = aMsgPart->bodyDecoded();
        ObjectTreeParser otp( this );
        otp.writeBodyStr( str,
                          overrideCodec() ? overrideCodec() : aMsgPart->codec(),
                          message() ? message()->from() : QString::null );
      }
      htmlWriter()->queue("</body></html>");
      htmlWriter()->flush();
      mMainWindow->setCaption(i18n("View Attachment: %1").arg(pname));
  } else if (kasciistricmp(aMsgPart->typeStr(), "image")==0 ||
             (kasciistricmp(aMsgPart->typeStr(), "application")==0 &&
              kasciistricmp(aMsgPart->subtypeStr(), "postscript")==0))
  {
      if (aFileName.isEmpty()) return;  // prevent crash
      // Open the window with a size so the image fits in (if possible):
      QImageIO *iio = new QImageIO();
      iio->setFileName(aFileName);
      if( iio->read() ) {
          QImage img = iio->image();
          QRect desk = KGlobalSettings::desktopGeometry(mMainWindow);
          // determine a reasonable window size
          int width, height;
          if( img.width() < 50 )
              width = 70;
          else if( img.width()+20 < desk.width() )
              width = img.width()+20;
          else
              width = desk.width();
          if( img.height() < 50 )
              height = 70;
          else if( img.height()+20 < desk.height() )
              height = img.height()+20;
          else
              height = desk.height();
          mMainWindow->resize( width, height );
      }
      // Just write the img tag to HTML:
      htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
      htmlWriter()->write( mCSSHelper->htmlHead( isFixedFont() ) );
      htmlWriter()->write( "<img src=\"file:" +
                           KURL::encode_string( aFileName ) +
                           "\" border=\"0\">\n"
                           "</body></html>\n" );
      htmlWriter()->end();
      setCaption( i18n("View Attachment: %1").arg( pname ) );
      show();
  } else {
    htmlWriter()->begin( mCSSHelper->cssDefinitions( isFixedFont() ) );
    htmlWriter()->queue( mCSSHelper->htmlHead( isFixedFont() ) );
    htmlWriter()->queue( "<pre>" );

    QString str = aMsgPart->bodyDecoded();
    // A QString cannot handle binary data. So if it's shorter than the
    // attachment, we assume the attachment is binary:
    if( str.length() < (unsigned) aMsgPart->decodedSize() ) {
      str.prepend( i18n("[KMail: Attachment contains binary data. Trying to show first character.]",
          "[KMail: Attachment contains binary data. Trying to show first %n characters.]",
          str.length()) + QChar('\n') );
    }
    htmlWriter()->queue( QStyleSheet::escape( str ) );
    htmlWriter()->queue( "</pre>" );
    htmlWriter()->queue("</body></html>");
    htmlWriter()->flush();
    mMainWindow->setCaption(i18n("View Attachment: %1").arg(pname));
  }
  // ---Sven's view text, html and image attachments in html widget end ---
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotAtmView( int id, const QString& name )
{
  partNode* node = mRootNode ? mRootNode->findId( id ) : 0;
  if( node ) {
    mAtmCurrent = id;
    mAtmCurrentName = name;
    if ( mAtmCurrentName.isEmpty() )
      mAtmCurrentName = tempFileUrlFromPartNode( node ).path();

    KMMessagePart& msgPart = node->msgPart();
    QString pname = msgPart.fileName();
    if (pname.isEmpty()) pname=msgPart.name();
    if (pname.isEmpty()) pname=msgPart.contentDescription();
    if (pname.isEmpty()) pname="unnamed";
    // image Attachment is saved already
    if (kasciistricmp(msgPart.typeStr(), "message")==0) {
      atmViewMsg(&msgPart);
    } else if ((kasciistricmp(msgPart.typeStr(), "text")==0) &&
	       (kasciistricmp(msgPart.subtypeStr(), "x-vcard")==0)) {
      setMsgPart( &msgPart, htmlMail(), name, pname );
    } else {
      KMReaderMainWin *win = new KMReaderMainWin(&msgPart, htmlMail(),
          name, pname, overrideEncoding() );
      win->show();
    }
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::openAttachment( int id, const QString & name )
{
  mAtmCurrentName = name;
  mAtmCurrent = id;

  QString str, pname, cmd, fileName;

  partNode* node = mRootNode ? mRootNode->findId( id ) : 0;
  if( !node ) {
    kdWarning(5006) << "KMReaderWin::openAttachment - could not find node " << id << endl;
    return;
  }
  if ( mAtmCurrentName.isEmpty() )
    mAtmCurrentName = tempFileUrlFromPartNode( node ).path();

  KMMessagePart& msgPart = node->msgPart();
  if (kasciistricmp(msgPart.typeStr(), "message")==0)
  {
    atmViewMsg(&msgPart);
    return;
  }

  QCString contentTypeStr( msgPart.typeStr() + '/' + msgPart.subtypeStr() );
  KPIM::kAsciiToLower( contentTypeStr.data() );

  if ( qstrcmp( contentTypeStr, "text/x-vcard" ) == 0 ) {
    showVCard( &msgPart );
    return;
  }

  // determine the MIME type of the attachment
  KMimeType::Ptr mimetype;
  // prefer the value of the Content-Type header
  mimetype = KMimeType::mimeType( QString::fromLatin1( contentTypeStr ) );
  if ( mimetype->name() == "application/octet-stream" ) {
    // consider the filename if Content-Type is application/octet-stream
    mimetype = KMimeType::findByPath( name, 0, true /* no disk access */ );
  }
  if ( ( mimetype->name() == "application/octet-stream" )
       && msgPart.isComplete() ) {
    // consider the attachment's contents if neither the Content-Type header
    // nor the filename give us a clue
    mimetype = KMimeType::findByFileContent( name );
  }

  KService::Ptr offer =
    KServiceTypeProfile::preferredService( mimetype->name(), "Application" );

  QString open_text;
  QString filenameText = msgPart.fileName();
  if ( filenameText.isEmpty() )
    filenameText = msgPart.name();
  if ( offer ) {
    open_text = i18n("&Open with '%1'").arg( offer->name() );
  } else {
    open_text = i18n("&Open With...");
  }
  const QString text = i18n("Open attachment '%1'?\n"
                            "Note that opening an attachment may compromise "
                            "your system's security.")
                       .arg( filenameText );
  const int choice = KMessageBox::questionYesNoCancel( this, text,
      i18n("Open Attachment?"), KStdGuiItem::saveAs(), open_text,
      QString::fromLatin1("askSave") + mimetype->name() ); // dontAskAgainName

  if( choice == KMessageBox::Yes ) {		// Save
    mAtmUpdate = true;
    KMHandleAttachmentCommand* command = new KMHandleAttachmentCommand( node,
        message(), mAtmCurrent, mAtmCurrentName, KMHandleAttachmentCommand::Save,
        offer, this );
    connect( command, SIGNAL( showAttachment( int, const QString& ) ),
        this, SLOT( slotAtmView( int, const QString& ) ) );
    command->start();
  }
  else if( choice == KMessageBox::No ) {	// Open
    KMHandleAttachmentCommand::AttachmentAction action = ( offer ?
        KMHandleAttachmentCommand::Open : KMHandleAttachmentCommand::OpenWith );
    mAtmUpdate = true;
    KMHandleAttachmentCommand* command = new KMHandleAttachmentCommand( node,
        message(), mAtmCurrent, mAtmCurrentName, action, offer, this );
    connect( command, SIGNAL( showAttachment( int, const QString& ) ),
        this, SLOT( slotAtmView( int, const QString& ) ) );
    command->start();
  } else {					// Cancel
    kdDebug(5006) << "Canceled opening attachment" << endl;
  }
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotScrollUp()
{
  if ( mWebPart ) {
    emit webScrollBy( 0, -10 );
    return;
  }
  static_cast<QScrollView *>(mViewer->widget())->scrollBy(0, -10);
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotScrollDown()
{
  if ( mWebPart ) {
    emit webScrollBy( 0, 10 );
    return;
  }
  static_cast<QScrollView *>(mViewer->widget())->scrollBy(0, 10);
}

bool KMReaderWin::atBottom() const
{
    if ( mWebPart )
      // as the part's script last reported it
      return mWebPart->property( "mailAtBottom" ).toBool();
    const QScrollView *view = static_cast<const QScrollView *>(mViewer->widget());
    return view->contentsY() + view->visibleHeight() >= view->contentsHeight();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotJumpDown()
{
    if ( mWebPart ) {
      emit webScrollBy( 0, QMAX( mWebPart->widget()->height() - 30, 10 ) );
      return;
    }
    QScrollView *view = static_cast<QScrollView *>(mViewer->widget());
    int offs = (view->clipper()->height() < 30) ? view->clipper()->height() : 30;
    view->scrollBy( 0, view->clipper()->height() - offs );
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotScrollPrior()
{
  if ( mWebPart ) {
    emit webScrollBy( 0, -(int)(height()*0.8) );
    return;
  }
  static_cast<QScrollView *>(mViewer->widget())->scrollBy(0, -(int)(height()*0.8));
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotScrollNext()
{
  if ( mWebPart ) {
    emit webScrollBy( 0, (int)(height()*0.8) );
    return;
  }
  static_cast<QScrollView *>(mViewer->widget())->scrollBy(0, (int)(height()*0.8));
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotDocumentChanged()
{

}


//-----------------------------------------------------------------------------
void KMReaderWin::slotTextSelected(bool)
{
  QString temp = mViewer->selectedText();
  kapp->clipboard()->setText(temp);
}

//-----------------------------------------------------------------------------
void KMReaderWin::selectAll()
{
  if ( mWebPart ) {
    emit webSelectAll();
    return;
  }
  mViewer->selectAll();
}

//-----------------------------------------------------------------------------
QString KMReaderWin::copyText()
{
  if ( mWebPart )
    return mWebPart->property( "mailSelectedText" ).toString();
  QString temp = mViewer->selectedText();
  return temp;
}


//-----------------------------------------------------------------------------
void KMReaderWin::slotDocumentDone()
{
  // mSbVert->setValue(0);
}


//-----------------------------------------------------------------------------
void KMReaderWin::setHtmlOverride(bool override)
{
  mHtmlOverride = override;
  if (message())
      message()->setDecodeHTML(htmlMail());
}


//-----------------------------------------------------------------------------
void KMReaderWin::setHtmlLoadExtOverride(bool override)
{
  mHtmlLoadExtOverride = override;
  //if (message())
  //    message()->setDecodeHTML(htmlMail());
}


//-----------------------------------------------------------------------------
bool KMReaderWin::htmlMail()
{
  return ((mHtmlMail && !mHtmlOverride) || (!mHtmlMail && mHtmlOverride));
}


//-----------------------------------------------------------------------------
bool KMReaderWin::htmlLoadExternal()
{
  return ((mHtmlLoadExternal && !mHtmlLoadExtOverride) ||
          (!mHtmlLoadExternal && mHtmlLoadExtOverride));
}


//-----------------------------------------------------------------------------
void KMReaderWin::saveRelativePosition()
{
  if ( mWebPart ) {
    // not kept with WebKit (yet): the message shows from its top
    mSavedRelativePosition = 0;
    return;
  }
  const QScrollView * scrollview = static_cast<QScrollView *>( mViewer->widget() );
  mSavedRelativePosition =
    static_cast<float>( scrollview->contentsY() ) / scrollview->contentsHeight();
}


//-----------------------------------------------------------------------------
void KMReaderWin::update( bool force )
{
  KMMessage* msg = message();
  if ( msg )
    setMsg( msg, force );
}


//-----------------------------------------------------------------------------
KMMessage* KMReaderWin::message( KMFolder** aFolder ) const
{
  KMFolder*  tmpFolder;
  KMFolder*& folder = aFolder ? *aFolder : tmpFolder;
  folder = 0;
  if (mMessage)
      return mMessage;
  if (mLastSerNum) {
    KMMessage *message = 0;
    int index;
    KMMsgDict::instance()->getLocation( mLastSerNum, &folder, &index );
    if (folder )
      message = folder->getMsg( index );
    if (!message)
      kdWarning(5006) << "Attempt to reference invalid serial number " << mLastSerNum << "\n" << endl;
    return message;
  }
  return 0;
}



//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlClicked()
{
  KMMainWidget *mainWidget = dynamic_cast<KMMainWidget*>(mMainWindow);
  uint identity = 0;
  if ( message() && message()->parent() ) {
    identity = message()->parent()->identity();
  }

  KMCommand *command = new KMUrlClickedCommand( mUrlClicked, identity, this,
						false, mainWidget );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotMailtoCompose()
{
  KMCommand *command = new KMMailtoComposeCommand( mUrlClicked, message() );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotMailtoForward()
{
  KMCommand *command = new KMMailtoForwardCommand( mMainWindow, mUrlClicked,
						   message() );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotMailtoAddAddrBook()
{
  KMCommand *command = new KMMailtoAddAddrBookCommand( mUrlClicked,
						       mMainWindow);
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotMailtoOpenAddrBook()
{
  KMCommand *command = new KMMailtoOpenAddrBookCommand( mUrlClicked,
							mMainWindow );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlCopy()
{
  // we don't necessarily need a mainWidget for KMUrlCopyCommand so
  // it doesn't matter if the dynamic_cast fails.
  KMCommand *command =
    new KMUrlCopyCommand( mUrlClicked,
                          dynamic_cast<KMMainWidget*>( mMainWindow ) );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlOpen( const KURL &url )
{
  if ( !url.isEmpty() )
    mUrlClicked = url;
  KMCommand *command = new KMUrlOpenCommand( mUrlClicked, this );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotAddBookmarks()
{
    KMCommand *command = new KMAddBookmarksCommand( mUrlClicked, this );
    command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotUrlSave()
{
  KMCommand *command = new KMUrlSaveCommand( mUrlClicked, mMainWindow );
  command->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotMailtoReply()
{
  KMCommand *command = new KMMailtoReplyCommand( mMainWindow, mUrlClicked,
    message(), copyText() );
  command->start();
}

//-----------------------------------------------------------------------------
partNode * KMReaderWin::partNodeFromUrl( const KURL & url ) {
  return mRootNode ? mRootNode->findId( msgPartFromUrl( url ) ) : 0 ;
}

partNode * KMReaderWin::partNodeForId( int id ) {
  return mRootNode ? mRootNode->findId( id ) : 0 ;
}


KURL KMReaderWin::tempFileUrlFromPartNode( const partNode * node )
{
  if (!node) return KURL();
  QStringList::const_iterator it = mTempFiles.begin();
  QStringList::const_iterator end = mTempFiles.end();

  while ( it != end ) {
      QString path = *it;
      it++;
      uint right = path.findRev('/');
      uint left = path.findRev('.', right);

      bool ok;
      int res = path.mid(left + 1, right - left - 1).toInt(&ok);
      if ( res == node->nodeId() )
          return KURL( path );
  }
  return KURL();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotSaveAttachments()
{
  mAtmUpdate = true;
  KMSaveAttachmentsCommand *saveCommand = new KMSaveAttachmentsCommand( mMainWindow,
                                                                        message() );
  saveCommand->start();
}

//-----------------------------------------------------------------------------
void KMReaderWin::slotSaveMsg()
{
  KMSaveMsgCommand *saveCommand = new KMSaveMsgCommand( mMainWindow, message() );

  if (saveCommand->url().isEmpty())
    delete saveCommand;
  else
    saveCommand->start();
}
//-----------------------------------------------------------------------------
void KMReaderWin::slotIMChat()
{
  KMCommand *command = new KMIMChatCommand( mUrlClicked, message() );
  command->start();
}

//-----------------------------------------------------------------------------
bool KMReaderWin::eventFilter( QObject *, QEvent *e )
{
  if ( e->type() == QEvent::MouseButtonPress ) {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if ( me->button() == LeftButton && ( me->state() & ShiftButton ) ) {
      // special processing for shift+click
      mAtmCurrent = msgPartFromUrl( mUrlClicked );
      if ( mAtmCurrent < 0 ) return false; // not an attachment
      mAtmCurrentName = mUrlClicked.path();
      slotHandleAttachment( KMHandleAttachmentCommand::Save ); // save
      return true; // eat event
    }
  }
  // standard event processing
  return false;
}

void KMReaderWin::slotDeleteAttachment(partNode * node)
{
  if ( KMessageBox::warningContinueCancel( this,
       i18n("Deleting an attachment might invalidate any digital signature on this message."),
       i18n("Delete Attachment"), KStdGuiItem::del(), "DeleteAttachmentSignatureWarning" )
     != KMessageBox::Continue ) {
    return;
  }
  KMDeleteAttachmentCommand* command = new KMDeleteAttachmentCommand( node, message(), this );
  command->start();
}

void KMReaderWin::slotEditAttachment(partNode * node)
{
  if ( KMessageBox::warningContinueCancel( this,
        i18n("Modifying an attachment might invalidate any digital signature on this message."),
        i18n("Edit Attachment"), KGuiItem( i18n("Edit"), "edit" ), "EditAttachmentSignatureWarning" )
        != KMessageBox::Continue ) {
    return;
  }
  KMEditAttachmentCommand* command = new KMEditAttachmentCommand( node, message(), this );
  command->start();
}

KMail::CSSHelper* KMReaderWin::cssHelper()
{
  return mCSSHelper;
}

bool KMReaderWin::decryptMessage() const
{
  if ( !GlobalSettings::self()->alwaysDecrypt() )
    return mDecrytMessageOverwrite;
  return true;
}

void KMReaderWin::useKHTML()
{
  if ( !mWebPart )
    return;
  if ( mHtmlWriter == mWebWriter )
    mHtmlWriter = 0;
  delete mWebWriter;
  mWebWriter = 0;
  delete mWebPart;
  mWebPart = 0;
  mViewer->widget()->show();
  mBox->setStretchFactor( mViewer->widget(), 1 );
  mHtmlWriter = new KHtmlPartHtmlWriter( mViewer, 0 );
}

void KMReaderWin::slotHideEmptyReaderParts()
{
  if ( message() )
    return;
  mColorBar->hide();
  mMimePartTree->hide();
  mMimePartTree->clear();
}

void KMReaderWin::injectAttachments()
{
  // inject attachments in header view
  // we have to do that after the otp has run so we also see encrypted parts
  DOM::Document doc = mViewer->htmlDocument();
  DOM::Element injectionPoint = doc.getElementById( "attachmentInjectionPoint" );
  if ( injectionPoint.isNull() )
    return;

  QString html = attachmentListHtml();
  if ( html.isEmpty() )
    return;

  assert( injectionPoint.tagName() == "div" );
  static_cast<DOM::HTMLElement>( injectionPoint ).setInnerHTML( html );
}

QString KMReaderWin::attachmentListHtml()
{
  QString imgpath( locate("data","kmail/pics/") );
  QString visibility;
  QString urlHandle;
  QString imgSrc;
  if( !showAttachmentQuicklist() )
    {
      urlHandle.append( "kmail:showAttachmentQuicklist" );
      imgSrc.append( "attachmentQuicklistClosed.png" );
    } else {
      urlHandle.append( "kmail:hideAttachmentQuicklist" );
      imgSrc.append( "attachmentQuicklistOpened.png" );
    }

  QString html = renderAttachments( mRootNode, QApplication::palette().active().background() );
  if ( html.isEmpty() )
    return html;

  if ( headerStyle() == HeaderStyle::fancy() )
    html.prepend( QString::fromLatin1("<div style=\"float:left;\">%1&nbsp;</div>" ).arg(i18n("Attachments:")) );

  if ( headerStyle() == HeaderStyle::enterprise() ) {
    QString link("");
    link += "<div style=\"text-align: right;\"><a href=\""+urlHandle+"\"><img src=\""+imgpath+imgSrc+"\"/></a></div>";
    html.prepend( link );
  }
  return html;
}

static QColor nextColor( const QColor & c )
{
  int h, s, v;
  c.hsv( &h, &s, &v );
  return QColor( (h + 50) % 360, QMAX(s, 64), v, QColor::Hsv );
}

QString KMReaderWin::renderAttachments(partNode * node, const QColor &bgColor )
{
  if ( !node )
    return QString();

  QString html;
  if ( node->firstChild() ) {
    QString subHtml = renderAttachments( node->firstChild(), nextColor( bgColor ) );
    if ( !subHtml.isEmpty() ) {

      QString visibility;
      if( !showAttachmentQuicklist() )
	{
	  visibility.append( "display:none;" );
	}

      QString margin;
      if ( node != mRootNode || headerStyle() != HeaderStyle::enterprise() )
        margin = "padding:2px; margin:2px; ";
      if ( node->msgPart().typeStr() == "message" || node == mRootNode )
        html += QString::fromLatin1("<div style=\"background:%1; %2"
            "vertical-align:middle; float:left; %3\">").arg( bgColor.name() ).arg( margin ).arg( visibility );
      html += subHtml;
      if ( node->msgPart().typeStr() == "message" || node == mRootNode )
        html += "</div>";
    }
  } else {
    QString label, icon;
    icon = node->msgPart().iconName( KIcon::Small );
    label = node->msgPart().contentDescription();
    if( label.isEmpty() )
      label = node->msgPart().name().stripWhiteSpace();
    if( label.isEmpty() )
      label = node->msgPart().fileName();
    bool typeBlacklisted = node->msgPart().typeStr() == "multipart";
    if ( !typeBlacklisted && node->msgPart().typeStr() == "application" ) {
      typeBlacklisted = node->msgPart().subtypeStr() == "pgp-encrypted"
          || node->msgPart().subtypeStr() == "pgp-signature"
          || node->msgPart().subtypeStr() == "pkcs7-mime"
          || node->msgPart().subtypeStr() == "pkcs7-signature";
    }
    typeBlacklisted = typeBlacklisted || node == mRootNode;
    // An image the HTML of a multipart/related shows: not an attachment.
    typeBlacklisted = typeBlacklisted || mHiddenResources.contains( node );
    if ( !label.isEmpty() && !icon.isEmpty() && !typeBlacklisted ) {
      html += "<div style=\"float:left;\">";
      html += QString::fromLatin1( "<span style=\"white-space:nowrap; border-width: 0px; border-left-width: 5px; border-color: %1; 2px; border-left-style: solid;\">" ).arg( bgColor.name() );
      html += QString::fromLatin1( "<a href=\"#att%1\">" ).arg( node->nodeId() );
      html += "<img style=\"vertical-align:middle;\" src=\"" + icon + "\"/>&nbsp;";
      // the label comes with the message (Content-Description, a file
      // name): text, not HTML
      if ( headerStyle() == HeaderStyle::enterprise() ) {
        QFont bodyFont = mCSSHelper->bodyFont( isFixedFont() );
        QFontMetrics fm( bodyFont );
        html += KMMessage::quoteHtmlChars( KStringHandler::rPixelSqueeze( label, fm, 140 ), true );
      } else
        html += KMMessage::quoteHtmlChars( label, true );
      html += "</a></span></div> ";
    }
  }

  html += renderAttachments( node->nextSibling(), nextColor ( bgColor ) );
  return html;
}

#include "kmreaderwin.moc"


