# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(printing)
add_subdirectory(korgac)
add_subdirectory(pixmaps)
add_subdirectory(sounds)
add_subdirectory(plugins)
add_subdirectory(interfaces)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/interfaces"
  "${PROJECT_SOURCE_DIR}/libemailfunctions"
  "${PROJECT_SOURCE_DIR}/libkpimidentities"
  "${PROJECT_SOURCE_DIR}/libkholidays"
  "${PROJECT_SOURCE_DIR}/mimelib"
  "${PROJECT_SOURCE_DIR}/certmanager/lib"
  "${PROJECT_SOURCE_DIR}/korganizer/printing"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
)

kde3_use_dcopidlng(../kmail/kmailIface.h)

kde3_add_library(korganizer_eventviewer SHARED VERSION_INFO 1:0
  SOURCES
    koeventviewer.cpp
    urihandler.cpp
    ../kmail/kmailIface.stub
    koeventviewerdialog.cpp
  MOC
    koeventviewer.h
    koeventviewerdialog.h
  LINK
    KDE3::kparts
    kcal
)

kde3_add_library(korganizer_calendar SHARED VERSION_INFO 1:0
  SOURCES
    stdcalendar.cpp
  LINK
    KDE3::kparts
    kdepim
    kcal
)

kde3_add_library(korganizer SHARED VERSION_INFO 1:0
  SOURCES
    komessagebox.cpp
    koagendaview.cpp
    koagenda.cpp
    koagendaitem.cpp
    datenavigator.cpp
    kdatenavigator.cpp
    datenavigatorcontainer.cpp
    datechecker.cpp
    komonthview.cpp
    searchdialog.cpp
    calendarview.cpp
    koviewmanager.cpp
    kodialogmanager.cpp
    archivedialog.cpp
    kohelper.cpp
    komailclient.cpp
    kotodoview.cpp
    kotodoviewitem.cpp
    kolistview.cpp
    koincidenceeditor.cpp
    koeventeditor.cpp
    kotodoeditor.cpp
    kojournaleditor.cpp
    koprefs.cpp
    kowindowlist.cpp
    koeditorgeneral.cpp
    koeditorgeneralevent.cpp
    koeditorgeneraltodo.cpp
    koeditorgeneraljournal.cpp
    koeditordetails.cpp
    koeditoralarms_base.ui
    koeditoralarms.cpp
    koeditorrecurrence.cpp
    koeditorattachments.cpp
    kogroupwareprefspage.ui
    koeventpopupmenu.cpp
    koeditorfreebusy.cpp
    publishdialog_base.ui
    publishdialog.cpp
    koeventview.cpp
    korganizeriface.skel
    kcalendariface.skel
    filtereditdialog.cpp
    filteredit_base.ui
    kowhatsnextview.cpp
    kocounterdialog.cpp
    kojournalview.cpp
    journalentry.cpp
    kocore.cpp
    mailscheduler.cpp
    kodaymatrix.cpp
    docprefs.cpp
    statusdialog.cpp
    koglobals.cpp
    konewstuff.cpp
    actionmanager.cpp
    resourceview.cpp
    navigatorbar.cpp
    kogroupware.cpp
    history.cpp
    koprefs_base.kcfgc
    koincidencetooltip.cpp
    aboutdata.cpp
    importdialog.cpp
    korganizerifaceimpl.cpp
    freebusymanager.cpp
    freebusyurldialog.cpp
    eventarchiver.cpp
    koidentitymanager.cpp
    exportwebdialog.cpp
    kocorehelper.cpp
    incidencechanger.cpp
    template_management_dialog_base.ui
    templatemanagementdialog.cpp
    agendaview.cpp
    multiagendaview.cpp
    timelineitem.cpp
    kotimelineview.cpp
    koattendeeeditor.cpp
    timelabels.cpp
  MOC
    actionmanager.h
    agendaview.h
    archivedialog.h
    interfaces/korganizer/baseview.h
    calendarview.h
    datechecker.h
    datenavigator.h
    datenavigatorcontainer.h
    eventarchiver.h
    exportwebdialog.h
    filtereditdialog.h
    freebusymanager.h
    freebusyurldialog.h
    history.h
    importdialog.h
    incidencechanger.h
    interfaces/korganizer/incidencechangerbase.h
    journalentry.h
    kdatenavigator.h
    koagenda.h
    koagendaitem.h
    koagendaview.h
    koattendeeeditor.h
    kocounterdialog.h
    kodaymatrix.h
    kodialogmanager.h
    koeditoralarms.h
    koeditorattachments.h
    koeditordetails.h
    koeditorfreebusy.h
    koeditorgeneral.h
    koeditorgeneralevent.h
    koeditorgeneraljournal.h
    koeditorgeneraltodo.h
    koeditorrecurrence.h
    koeventeditor.h
    koeventpopupmenu.h
    koeventview.h
    kogroupware.h
    koincidenceeditor.h
    kojournaleditor.h
    kojournalview.h
    kolistview.h
    komonthview.h
    kotimelineview.h
    kotodoeditor.h
    kotodoview.h
    koviewmanager.h
    kowhatsnextview.h
    kowindowlist.h
    multiagendaview.h
    navigatorbar.h
    publishdialog.h
    resourceview.h
    searchdialog.h
    statusdialog.h
    templatemanagementdialog.h
    timelabels.h
  LINK
    kcal
    kpimidentities
    kdepim
    korganizer_eventviewer
    korganizer_calendar
    kocorehelper
    korg_stdprinting
    kdgantt
    emailfunctions
    kholidays
    kmime
    KDE3::kparts
    KDE3::kio
    KDE3::knewstuff
    KDE3::kdeprint
    KDE3::kabc
    KDE3::kutils
)

kde3_add_module(kcm_korganizer
  SOURCES
    koprefsdialog.cpp
  MOC
    koprefsdialog.h
  LINK
    korganizer
    kdepim
    KDE3::kdecore
)

kde3_add_module(libkorganizerpart
  SOURCES
    korganizer_part.cpp
  MOC
    korganizer_part.h
  LINK
    korganizer
)

# target korganizer is libkorganizer
kde3_add_executable(korganizer_bin
  OUTPUT_NAME korganizer
  SOURCES
    main.cpp
    korganizer.cpp
    koapp.cpp
  MOC
    koapp.h
    korganizer.h
  LINK
    korganizer
)

install(FILES
  tips
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/korganizer)

# AC_OUTPUT(korganizer/ical2vcal) of configure.in.in
set(PERL "${PERL_EXECUTABLE}")
configure_file(ical2vcal.in ical2vcal @ONLY)
install(PROGRAMS
  "${CMAKE_CURRENT_BINARY_DIR}/ical2vcal"
  DESTINATION ${KDE3_BIN_INSTALL_DIR})

install(FILES
  korganizerui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/korganizer)

install(FILES
  korganizer_part.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/korganizer)

install(FILES
  dcopcalendar.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

install(FILES
  korganizer.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

install(FILES
  korganizer.kcfg
  DESTINATION ${KDE3_KCFG_INSTALL_DIR})

install(FILES
  korganizer_configmain.desktop
  korganizer_configtime.desktop
  korganizer_configviews.desktop
  korganizer_configcolors.desktop
  korganizer_configfonts.desktop
  korganizer_configplugins.desktop
  korganizer_configgroupscheduling.desktop
  korganizer_configfreebusy.desktop
  korganizer_configdesignerfields.desktop
  webcal.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})

install(FILES
  korganizer.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

kde3_install_icons(AUTO)

# install-data-local: hide the services of removed components
kdepim_install_uninstall_desktop(uninstall.desktop
  ${KDE3_SERVICES_INSTALL_DIR}/korganizer_configgroupautomation.desktop
  ${KDE3_SERVICES_INSTALL_DIR}/korganizer/webexport.desktop
  ${KDE3_SERVICES_INSTALL_DIR}/korganizer/holidays.desktop)
