include_directories(
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kabinterfaces SHARED VERSION_INFO 1:0:0
  SOURCES
    configurewidget.cpp
    contacteditorwidget.cpp
    core.cpp
    extensionwidget.cpp
    xxport.cpp
  MOC
    contacteditorwidget.h
    core.h
    extensionwidget.h
    xxport.h
  LINK
    KDE3::kabc
)

install(FILES
  configurewidget.h
  contacteditorwidget.h
  core.h
  extensionwidget.h
  xxport.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kaddressbook)

install(FILES
  kaddressbook_contacteditorwidget.desktop
  kaddressbook_extension.desktop
  kaddressbook_xxport.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
