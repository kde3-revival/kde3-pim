include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ui"
  "${CMAKE_CURRENT_BINARY_DIR}/../lib/ui"
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

add_compile_definitions(LIBKLEOPATRA_NO_COMPAT)

# libconf.la: the stuff that gets compiled into kleopatra
kde3_add_library(kleopatra_conf OBJECT
  SOURCES
    configuredialog.cpp
  MOC
    configuredialog.h
  LINK
    KDE3Qt::Qt
    PkgConfig::GPGME
)

kde3_add_module(kcm_kleopatra
  SOURCES
    dirservconfigpage.cpp
    appearanceconfigpage.cpp
    appearanceconfigwidgetbase.ui
    appearanceconfigwidget.cpp
    dnorderconfigpage.cpp
  MOC
    appearanceconfigpage.h
    appearanceconfigwidget.h
    dirservconfigpage.h
    dnorderconfigpage.h
  LINK
    kleopatra
    KDE3::kdeui
)

install(FILES
  kleopatra_config_dirserv.desktop
  kleopatra_config_appear.desktop
  kleopatra_config_dnorder.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
# dirservconfigpage.lo: ../lib/ui/directoryserviceswidgetbase.h
add_dependencies(kcm_kleopatra kleopatra_ui)
