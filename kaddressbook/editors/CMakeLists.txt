# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(protocols)
add_subdirectory(icons)

include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/certmanager/lib"
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
  "${PROJECT_BINARY_DIR}/kaddressbook/editors"
)

kde3_add_library(kabim OBJECT
  SOURCES
    imeditorwidget.cpp
    imeditorbase.ui
    imaddressbase.ui
    imaddresswidget.cpp
  MOC
    imaddresswidget.h
    imeditorwidget.h
  LINK
    kabinterfaces
)

kde3_add_module(libkaddrbk_cryptosettings
  SOURCES
    cryptowidget.cpp
  MOC
    cryptowidget.h
  LINK
    kabinterfaces
    kleopatra
)

install(FILES
  cryptosettings.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kaddressbook)

install(FILES
  kaddressbookimprotocol.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

