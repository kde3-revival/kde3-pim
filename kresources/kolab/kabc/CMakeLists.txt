include_directories(
  "${PROJECT_SOURCE_DIR}/kresources/kolab/shared"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kabckolab SHARED VERSION_INFO 0:0:0
  SOURCES
    resourcekolab.cpp
    contact.cpp
  MOC
    resourcekolab.h
  LINK
    resourcekolabshared
    KDE3::kresources
    KDE3::kabc
)

kde3_add_module(kabc_kolab
  SOURCES
    resourcekolab_plugin.cpp
  LINK
    kabckolab
)

install(FILES
  kolab.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kabc)


# install-data-local: hide the services of the old imap resource
kdepim_install_uninstall_desktop(../uninstall.desktop
  ${KDE3_SERVICES_INSTALL_DIR}/kresources/kabc/imap.desktop)
