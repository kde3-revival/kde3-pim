include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}"
)

kde3_use_dcopidlng(../../../kmail/kmailIface.h)

kde3_add_module(libkontact_kaddressbookplugin
  SOURCES
    kaddressbook_plugin.cpp
    ../../../kaddressbook/kaddressbookiface.stub
    ../../../kmail/kmailIface.stub
  MOC
    kaddressbook_plugin.h
  LINK
    kpinterfaces
    kdepim
    KDE3::kabc
)

install(FILES
  kaddressbookplugin.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kontact)
