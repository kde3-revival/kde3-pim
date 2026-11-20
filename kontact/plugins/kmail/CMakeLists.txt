include_directories(
  "${PROJECT_SOURCE_DIR}/kontact/interfaces"
  "${PROJECT_SOURCE_DIR}/kmail"
  "${PROJECT_BINARY_DIR}/kmail"
  "${PROJECT_SOURCE_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}"
)

kde3_use_dcopidlng(../../../kmail/kmailIface.h summarywidget.h)

kde3_add_module(libkontact_kmailplugin
  SOURCES
    kmail_plugin.cpp
    ../../../kmail/kmailIface.stub
    summarywidget.cpp
    summarywidget.skel
  MOC
    kmail_plugin.h
    summarywidget.h
  LINK
    kpinterfaces
    kcal
    KDE3::kparts
)

kde3_add_module(kcm_kmailsummary
  SOURCES
    kcmkmailsummary.cpp
  MOC
    kcmkmailsummary.h
  LINK
    KDE3::kdeui
)

install(FILES
  kmailplugin.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kontact)

install(FILES
  kcmkmailsummary.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})


# kmail_plugin.cpp includes kmail/kmkernel.h, which includes the
# generated globalsettings_base.h and templatesconfiguration_base.h of
# libkmailprivate; the plugin does not link it (autotools built kmail
# before kontact)
add_dependencies(libkontact_kmailplugin kmailprivate)
