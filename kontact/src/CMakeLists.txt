# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(about)

include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kontact SHARED VERSION_INFO 1:0
  SOURCES
    prefs.kcfgc
  LINK
    KDE3::kdecore
)

kde3_add_module(kcm_kontact
  SOURCES
    kcmkontact.cpp
  MOC
    kcmkontact.h
  LINK
    kontact
    kdepim
)

# target kontact is libkontact
kde3_add_executable(kontact_bin
  OUTPUT_NAME kontact
  SOURCES
    main.cpp
    mainwindow.cpp
    sidepanebase.cpp
    iconsidepane.cpp
    aboutdialog.cpp
    profilemanager.cpp
    profiledialog.cpp
    kontactiface.skel
  MOC
    aboutdialog.h
    iconsidepane.h
    mainwindow.h
    profiledialog.h
    profilemanager.h
    sidepanebase.h
  LINK
    kdepim
    kpinterfaces
    kontact
    KDE3::kparts
    KDE3::kutils
    KDE3::khtml
)

install(FILES
  kontactui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kontact)

install(FILES
  Kontact.desktop
  kontactdcop.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

install(FILES
  kontact.kcfg
  DESTINATION ${KDE3_KCFG_INSTALL_DIR})

install(FILES
  kontactconfig.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  kontact.setdlg
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kontact)

kde3_install_icons(AUTO)
