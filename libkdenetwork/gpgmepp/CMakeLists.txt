# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(interfaces)

include_directories(
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
  "${PROJECT_SOURCE_DIR}/libkdenetwork/gpgmepp"
)

kde3_add_library(gpgme++ SHARED VERSION_INFO 3:0:3
  SOURCES
    context.cpp
    key.cpp
    trustitem.cpp
    data.cpp
    callbacks.cpp
    eventloopinteractor.cpp
    keylistresult.cpp
    keygenerationresult.cpp
    importresult.cpp
    decryptionresult.cpp
    verificationresult.cpp
    signingresult.cpp
    encryptionresult.cpp
    engineinfo.cpp
  LINK
    PkgConfig::GPGME
)

install(FILES
  context.h
  key.h
  trustitem.h
  eventloopinteractor.h
  data.h
  gpgmefw.h
  result.h
  keylistresult.h
  keygenerationresult.h
  importresult.h
  decryptionresult.h
  verificationresult.h
  signingresult.h
  encryptionresult.h
  engineinfo.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/gpgme++)
