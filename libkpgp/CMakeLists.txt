# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(pics)

kde3_add_library(kpgp SHARED VERSION_INFO 4:0:2
  SOURCES
    kpgpblock.cpp
    kpgpui.cpp
    kpgpkey.cpp
    kpgp.cpp
    kpgpbase.cpp
    kpgpbaseG.cpp
    kpgpbase2.cpp
    kpgpbase5.cpp
    kpgpbase6.cpp
  MOC
    kpgpui.h
  LINK
    KDE3::kdeui
)

install(FILES
  kpgp.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

install(PROGRAMS
  kpgp-3.1-upgrade-address-data.pl
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
