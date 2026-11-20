# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(icons)
add_subdirectory(pics)
add_subdirectory(about)

include_directories(
  "${PROJECT_SOURCE_DIR}"
)

# AM_CXXFLAGS = -DQT_PLUGIN (libkdepim is compiled with it, too)
add_compile_definitions(QT_PLUGIN)

kde3_use_dcopidlng(interfaces/MailTransportServiceIface.h)

kde3_add_library(kdepim SHARED VERSION_INFO 1:0:0
  SOURCES
    ktimeedit.cpp
    alarmclient.cpp
    kprefsdialog.cpp
    kpimprefs.cpp
    categoryselectdialog_base.ui
    categoryselectdialog.cpp
    categoryeditdialog_base.ui
    categoryeditdialog.cpp
    kdateedit.cpp
    kimportdialog.cpp
    kvcarddrag.cpp
    sidebarextension.cpp
    infoextension.cpp
    addressesdialog.cpp
    addresspicker.ui
    addresseeview.cpp
    maillistdrag.cpp
    interfaces/MailTransportServiceIface.skel
    interfaces/AddressBookServiceIface.skel
    kconfigpropagator.cpp
    kconfigwizard.cpp
    weaver.cpp
    weaverextensions.cpp
    weaverlogger.cpp
    pluginloaderbase.cpp
    addresseelineedit.cpp
    addresseelineedit.skel
    kdatepickerpopup.cpp
    kfileio.cpp
    clicklineedit.cpp
    configmanager.cpp
    collectingprocess.cpp
    ldapclient.cpp
    overlaywidget.cpp
    progressmanager.cpp
    progressdialog.cpp
    statusbarprogresswidget.cpp
    ssllabel.cpp
    completionordereditor.cpp
    resourceabc.cpp
    diffalgo.cpp
    addresseediffalgo.cpp
    calendardiffalgo.cpp
    htmldiffalgodisplay.cpp
    ldapsearchdialog.cpp
    broadcaststatus.cpp
    kresourceprefs.cpp
    kpixmapregionselectorwidget.cpp
    kpixmapregionselectordialog.cpp
    kabcresourcecached.cpp
    kxface.cpp
    kaccount.cpp
    kaddrbook.cpp
    kfoldertree.cpp
    kregexp3.cpp
    kscoring.cpp
    kscoringeditor.cpp
    ksubscription.cpp
    kwidgetlister.cpp
    linklocator.cpp
    qutf7codec.cpp
    recentaddresses.cpp
    spellingfilter.cpp
    addresseeselector.cpp
    addresseeemailselection.cpp
    designerfields.cpp
    kcmdesignerfields.cpp
    embeddedurlpage.cpp
    kincidencechooser.cpp
    groupwarejob.cpp
    pimemoticons.kcfgc
    krsqueezedtextlabel.cpp
    csshelper.cpp
    distributionlist.cpp
    kpimurlrequesterdlg.cpp
    sendsmsdialog.cpp
    kmailcompletion.cpp
  MOC
    addresseelineedit.h
    addresseeselector.h
    addresseeview.h
    addressesdialog.h
    broadcaststatus.h
    categoryeditdialog.h
    categoryselectdialog.h
    clicklineedit.h
    collectingprocess.h
    completionordereditor.h
    configmanager.h
    designerfields.h
    embeddedurlpage.h
    infoextension.h
    kabcresourcecached.h
    kcmdesignerfields.h
    kconfigwizard.h
    kdateedit.h
    kdatepickerpopup.h
    kfoldertree.h
    kimportdialog.h
    kincidencechooser.h
    kmailcompletion.h
    kpixmapregionselectorwidget.h
    kprefsdialog.h
    krsqueezedtextlabel.h
    kscoring.h
    kscoringeditor.h
    ksubscription.h
    ktimeedit.h
    kvcarddrag.h
    kwidgetlister.h
    kxface.h
    ldapclient.h
    ldapsearchdialog.h
    overlaywidget.h
    progressdialog.h
    progressmanager.h
    resourceabc.h
    sendsmsdialog.h
    sidebarextension.h
    statusbarprogresswidget.h
    weaver.h
    weaverextensions.h
    weaverlogger.h
  LINK
    kcal
    emailfunctions
    KDE3::kio
    KDE3::kabc
    KDE3::kparts
    KDE3::kimproxy
    KDE3Qt::QUI
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kdepimwidgets.cpp"
  COMMAND ${KDE3_TOOL_LAUNCHER} "$<TARGET_FILE:KDE3::makekdewidgets>"
          -o "${CMAKE_CURRENT_BINARY_DIR}/kdepimwidgets.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/kdepim.widgets"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/kdepim.widgets" "$<TARGET_FILE:KDE3::makekdewidgets>"
  COMMENT "Generating kdepimwidgets.cpp"
  VERBATIM)

kde3_add_module(kdepimwidgets
  DESTINATION ${KDE3_WIDGET_INSTALL_DIR}
  SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/kdepimwidgets.cpp"
  LINK
    KDE3::kio
    kdepim
)

install(FILES
  kdepimmacros.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR})

install(FILES
  pimemoticons.kcfg
  DESTINATION ${KDE3_KCFG_INSTALL_DIR})

