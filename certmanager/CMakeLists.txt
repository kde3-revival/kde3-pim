# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(lib)
add_subdirectory(conf)
add_subdirectory(kwatchgnupg)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib"
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

# target kleopatra is lib/libkleopatra
kde3_add_executable(kleopatra_bin
  OUTPUT_NAME kleopatra
  SOURCES
    customactions.cpp
    aboutdata.cpp
    main.cpp
    certmanager.cpp
    hierarchyanalyser.cpp
    certificatewizard.ui
    certificatewizardimpl.cpp
    certificateinfowidget.ui
    certificateinfowidgetimpl.cpp
    crlview.cpp
    storedtransferjob.cpp
    certlistview.cpp
  MOC
    certificateinfowidgetimpl.h
    certificatewizardimpl.h
    certlistview.h
    certmanager.h
    crlview.h
    customactions.h
    hierarchyanalyser.h
    storedtransferjob.h
  LINK
    kleopatra_conf
    kleopatra
    KDE3::kutils
    KDE3::kabc
)
# KDE_CXXFLAGS = -DLIBKLEOPATRA_NO_COMPAT
target_compile_definitions(kleopatra_bin PRIVATE LIBKLEOPATRA_NO_COMPAT)

install(FILES
  kleopatraui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kleopatra)

install(FILES
  kleopatra_import.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})
