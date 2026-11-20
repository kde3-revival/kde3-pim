# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(shared)
add_subdirectory(kabc)
add_subdirectory(kcal)

install(FILES
  kolab-resource.upd
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)

install(PROGRAMS
  upgrade-resourcetype.pl
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kconf_update)
