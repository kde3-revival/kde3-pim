# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(interfaces)
add_subdirectory(common)
add_subdirectory(printing)
add_subdirectory(editors)
add_subdirectory(views)
add_subdirectory(features)
add_subdirectory(xxport)
add_subdirectory(kcmconfigs)
add_subdirectory(pics)
add_subdirectory(csv-templates)
add_subdirectory(thumbnailcreator)

include_directories(
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}/kaddressbook/features"
  "${PROJECT_SOURCE_DIR}/kaddressbook/views"
  "${PROJECT_SOURCE_DIR}/kaddressbook/printing"
  "${PROJECT_SOURCE_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}/kaddressbook/editors"
  "${PROJECT_BINARY_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}"
)
# -I$(top_buildir)/kaddressbook/editors: top_buildir is no variable, so
# this was -I/kaddressbook/editors

kde3_add_library(kaddressbook SHARED VERSION_INFO 0:0:0
  SOURCES
    addresseeconfig.cpp
    addresseeeditordialog.cpp
    addresseeeditorwidget.cpp
    addresseeeditorbase.cpp
    addresseeeditorextension.cpp
    addresseeutil.cpp
    addresseditwidget.cpp
    addviewdialog.cpp
    emaileditwidget.cpp
    extensionmanager.cpp
    filtereditdialog.cpp
    filterselectionwidget.cpp
    geowidget.cpp
    imagewidget.cpp
    incsearchwidget.cpp
    jumpbuttonbar.cpp
    kabcore.cpp
    kablock.cpp
    kaddressbookiface.skel
    kaddressbookservice.cpp
    kaddressbookview.cpp
    keywidget.cpp
    ldapsearchdialog.cpp
    nameeditdialog.cpp
    phoneeditwidget.cpp
    secrecywidget.cpp
    soundwidget.cpp
    viewconfigurefieldspage.cpp
    viewconfigurefilterpage.cpp
    viewconfigurewidget.cpp
    undocmds.cpp
    viewmanager.cpp
    xxportmanager.cpp
    xxportselectdialog.cpp
    contacteditorwidgetmanager.cpp
    simpleaddresseeeditor.cpp
    advancedcustomfields.cpp
    customfieldswidget.cpp
    freebusywidget.cpp
    searchmanager.cpp
    imeditwidget.cpp
    kabtools.cpp
    distributionlistpicker.cpp
    distributionlisteditor.cpp
    distributionlistentryview.cpp
  MOC
    addresseditwidget.h
    addresseeeditorbase.h
    addresseeeditordialog.h
    addresseeeditorextension.h
    addresseeeditorwidget.h
    addviewdialog.h
    advancedcustomfields.h
    contacteditorwidgetmanager.h
    customfieldswidget.h
    distributionlisteditor.h
    distributionlisteditor_p.h
    distributionlistentryview.h
    distributionlistpicker.h
    emaileditwidget.h
    extensionmanager.h
    filtereditdialog.h
    filterselectionwidget.h
    freebusywidget.h
    geowidget.h
    imagewidget.h
    imeditwidget.h
    incsearchwidget.h
    jumpbuttonbar.h
    kabcore.h
    kaddressbookview.h
    keywidget.h
    ldapsearchdialog.h
    nameeditdialog.h
    phoneeditwidget.h
    searchmanager.h
    secrecywidget.h
    simpleaddresseeeditor.h
    soundwidget.h
    viewconfigurefieldspage.h
    viewconfigurefilterpage.h
    viewconfigurewidget.h
    viewmanager.h
    xxportmanager.h
    xxportselectdialog.h
  LINK
    printer
    kabinterfaces
    kdepim
    kcal
    KDE3::kabc_file
    KDE3::kabc
    KDE3::khtml
    kabcommon
    kabim
    KDE3::kdeprint
)

kde3_add_module(libkaddressbookpart
  SOURCES
    kaddressbook_part.cpp
  MOC
    kaddressbook_part.h
  LINK
    kaddressbook
)

# target kaddressbook is libkaddressbook
kde3_add_executable(kaddressbook_bin
  OUTPUT_NAME kaddressbook
  SOURCES
    main.cpp
    kaddressbookmain.cpp
  MOC
    kaddressbookmain.h
  LINK
    kaddressbook
)

install(FILES
  kaddressbookui.rc
  kaddressbook_part.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kaddressbook)

install(FILES
  zone.tab
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kaddressbook)

install(FILES
  kaddressbook.desktop
  DESTINATION ${KDE3_XDG_APPS_INSTALL_DIR})

install(FILES
  kaddressbook_view.desktop
  dcopaddressbook.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})

kde3_install_icons(kaddressbook)


# kaddressbook_part.lo: common/kabprefs_base.h
add_dependencies(libkaddressbookpart kabcommon)
