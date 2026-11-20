kde3_add_module(kio_mbox
  SOURCES
    mbox.cc
    mboxfile.cc
    readmbox.cc
    stat.cc
    urlinfo.cc
  LINK
    KDE3::kio
)

install(FILES
  mbox.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
