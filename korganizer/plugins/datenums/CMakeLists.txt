include_directories(
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
  "${PROJECT_SOURCE_DIR}/korganizer"
)

kde3_add_module(libkorg_datenums
  SOURCES
    configdialog.cpp
    datenums.cpp
  MOC
    configdialog.h
  LINK
    korganizer
    KDE3::kdeui
)

install(FILES
  datenums.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/korganizer)
