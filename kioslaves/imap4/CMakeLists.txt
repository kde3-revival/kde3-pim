include_directories(
  "${PROJECT_SOURCE_DIR}/libkmime"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${PROJECT_SOURCE_DIR}/libemailfunctions"
)

kde3_add_module(kio_imap4
  SOURCES
    imapcommand.cc
    imaplist.cc
    mailaddress.cc
    mimeheader.cc
    rfcdecoder.cc
    imap4.cc
    imapinfo.cc
    imapparser.cc
    mailheader.cc
    mimehdrline.cc
    mimeio.cc
  LINK
    KDE3::kio
    ${SASL2_LIBS}
    kmime
    emailfunctions
)

install(FILES
  imap4.protocol
  imaps.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
