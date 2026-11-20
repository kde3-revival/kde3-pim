include_directories(
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
)

kde3_add_module(libkorg_hebrew
  SOURCES
    hebrew.cpp
    configdialog.cpp
    holiday.cpp
    parsha.cpp
    converter.cpp
  MOC
    configdialog.h
  LINK
    KDE3::kdecore
    KDE3::kdeui
)

install(FILES
  hebrew.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/korganizer)
