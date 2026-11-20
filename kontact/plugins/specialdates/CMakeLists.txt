include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_module(libkontact_specialdatesplugin
  SOURCES
    specialdates_plugin.cpp
    sdsummarywidget.cpp
  MOC
    sdsummarywidget.h
  LINK
    kpinterfaces
    kdepim
    kholidays
    korganizer_calendar
    KDE3::kabc
)

kde3_add_module(kcm_sdsummary
  SOURCES
    kcmsdsummary.cpp
  MOC
    kcmsdsummary.h
  LINK
    KDE3::kdeui
)

install(FILES
  specialdatesplugin.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kontact)

install(FILES
  kcmsdsummary.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
