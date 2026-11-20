include_directories(
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kcal_resourceremote SHARED VERSION_INFO 1:0:0
  SOURCES
    resourceremote.cpp
    resourceremoteconfig.cpp
  MOC
    resourceremote.h
    resourceremoteconfig.h
  LINK
    kcal
    kdepim
)

kde3_add_module(kcal_remote
  SOURCES
    resourceremote_plugin.cpp
  LINK
    kcal_resourceremote
    kdepim
)

install(FILES
  resourceremoteconfig.h
  resourceremote.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/libkcal)

install(FILES
  remote.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kcal)
