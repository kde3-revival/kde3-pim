include_directories(
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

kde3_add_library(qgpgme SHARED VERSION_INFO 0:0:0
  SOURCES
    eventloopinteractor.cpp
    dataprovider.cpp
  MOC
    eventloopinteractor.h
  LINK
    KDE3Qt::Qt
    gpgme++
)

install(FILES
  eventloopinteractor.h
  dataprovider.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/qgpgme)
