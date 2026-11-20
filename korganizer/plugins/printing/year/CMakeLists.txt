include_directories(
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
  "${PROJECT_SOURCE_DIR}/korganizer/printing"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_module(libkorg_yearlyprint
  SOURCES
    calprintyearconfig_base.ui
    yearprint.cpp
  LINK
    KDE3::kdecore
    KDE3::kdeui
    korg_stdprinting
)

install(FILES
  yearprint.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/korganizer)
