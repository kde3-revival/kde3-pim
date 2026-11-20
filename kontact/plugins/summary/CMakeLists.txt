include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}/certmanager/lib"
  "${PROJECT_SOURCE_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_module(libkontact_summaryplugin
  SOURCES
    summaryview_plugin.cpp
    summaryview_part.cpp
    dropwidget.cpp
  MOC
    dropwidget.h
    summaryview_part.h
    summaryview_plugin.h
  LINK
    KDE3::kutils
    kpinterfaces
    KDE3::kparts
    kdepim
    kpimidentities
)

kde3_add_module(kcm_kontactsummary
  SOURCES
    kcmkontactsummary.cpp
  MOC
    kcmkontactsummary.h
  LINK
    KDE3::kdeui
    KDE3::kutils
)

install(FILES
  summaryplugin.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kontact)

install(FILES
  kcmkontactsummary.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  kontactsummary_part.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kontactsummary)
