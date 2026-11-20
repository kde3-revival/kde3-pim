include_directories(
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
  "${PROJECT_SOURCE_DIR}/korganizer/printing"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_module(libkorg_journalprint
  SOURCES
    calprintjournalconfig_base.ui
    journalprint.cpp
  LINK
    KDE3::kdecore
    KDE3::kdeui
    korg_stdprinting
)

install(FILES
  journalprint.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/korganizer)
