# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(pics)

kde3_add_executable(kmailcvt
  SOURCES
    kimportpagedlg.ui
    kimportpage.cpp
    kselfilterpagedlg.ui
    kselfilterpage.cpp
    filters.cxx
    filter_oe.cxx
    kmailcvt.cpp
    main.cpp
    filter_pmail.cxx
    filter_plain.cxx
    filter_outlook.cxx
    filter_mbox.cxx
    filter_evolution.cxx
    filter_mailapp.cxx
    filter_evolution_v2.cxx
    filter_opera.cxx
    filter_thunderbird.cxx
    filter_kmail_maildir.cxx
    filter_sylpheed.cxx
    filter_thebat.cxx
    filter_lnotes.cxx
  MOC
    kimportpage.h
    kmailcvt.h
    kselfilterpage.h
  LINK
    KDE3::kio
)

kde3_install_icons(AUTO)

# install-data-local: hide the old menu entry
kdepim_install_uninstall_desktop(uninstall.desktop
  ${KDE3_APPS_INSTALL_DIR}/Utilities/kmailcvt.desktop)
