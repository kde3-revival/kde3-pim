include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}/korganizer"
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
  "${PROJECT_SOURCE_DIR}"
)

kde3_use_dcopidlng(../../../korganizer/korganizeriface.h)

# libcommon.la
kde3_add_library(kontact_korganizer_common OBJECT
  SOURCES
    korg_uniqueapp.cpp
  LINK
    KDE3::kparts
)

kde3_add_module(libkontact_korganizerplugin
  SOURCES
    korganizerplugin.cpp
    ../../../korganizer/kcalendariface.stub
    summarywidget.cpp
    ../../../korganizer/korganizeriface.stub
  MOC
    korganizerplugin.h
    summarywidget.h
  LINK
    kontact_korganizer_common
    kpinterfaces
    KDE3::kparts
    korganizer_calendar
)

kde3_add_module(libkontact_todoplugin
  SOURCES
    todoplugin.cpp
    ../../../korganizer/kcalendariface.stub
    todosummarywidget.cpp
    ../../../korganizer/korganizeriface.stub
  MOC
    todoplugin.h
    todosummarywidget.h
  LINK
    kontact_korganizer_common
    kpinterfaces
    KDE3::kparts
    korganizer_calendar
    korganizer
)

kde3_add_module(libkontact_journalplugin
  SOURCES
    journalplugin.cpp
    ../../../korganizer/kcalendariface.stub
  MOC
    journalplugin.h
  LINK
    kontact_korganizer_common
    kpinterfaces
    KDE3::kparts
    korganizer_calendar
)

kde3_add_module(kcm_korgsummary
  SOURCES
    kcmkorgsummary.cpp
  MOC
    kcmkorgsummary.h
  LINK
    KDE3::kdeui
)

install(FILES
  korganizerplugin.desktop
  todoplugin.desktop
  journalplugin.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kontact)

install(FILES
  kcmkorgsummary.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
