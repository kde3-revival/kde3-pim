include_directories(
  "${PROJECT_SOURCE_DIR}/ktnef"
)

kde3_add_library(ktnef SHARED VERSION_INFO 1:0:0
  SOURCES
    ktnefparser.cpp
    ktnefproperty.cpp
    ktnefattach.cpp
    mapi.cpp
    ktnefpropertyset.cpp
    ktnefmessage.cpp
    ktnefwriter.cpp
    lzfu.cpp
  LINK
    KDE3::kdecore
    KDE3::kio
)

install(FILES
  ms-tnef.desktop
  DESTINATION ${KDE3_MIME_INSTALL_DIR}/application)
