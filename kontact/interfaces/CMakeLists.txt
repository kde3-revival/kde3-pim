include_directories(
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kpinterfaces SHARED VERSION_INFO 1:0:0
  SOURCES
    core.cpp
    plugin.cpp
    summary.cpp
    uniqueapphandler.cpp
  MOC
    core.h
    plugin.h
    summary.h
    uniqueapphandler.h
  LINK
    kdepim
    KDE3::kparts
)

install(FILES
  core.h
  plugin.h
  summary.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/kontact)

install(FILES
  kontactplugin.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
