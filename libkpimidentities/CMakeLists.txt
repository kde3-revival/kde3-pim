include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/certmanager/lib"
)

kde3_use_dcopidlng(identitymanager.h)

kde3_add_library(kpimidentities SHARED VERSION_INFO 1:0:0
  SOURCES
    identitymanager.cpp
    identity.cpp
    identitymanager.skel
    identitycombo.cpp
  MOC
    identitycombo.h
    identitymanager.h
  LINK
    kleopatra
    kdepim
    emailfunctions
)

