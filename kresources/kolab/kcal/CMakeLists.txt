include_directories(
  "${PROJECT_SOURCE_DIR}/kresources/kolab/shared"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/libkdepim"
)

kde3_add_library(kcalkolab SHARED VERSION_INFO 0:0:0
  SOURCES
    incidence.cpp
    event.cpp
    task.cpp
    journal.cpp
    resourcekolab.cpp
  MOC
    resourcekolab.h
  LINK
    kcal
    resourcekolabshared
    KDE3::kresources
)

kde3_add_module(kcal_kolab
  SOURCES
    resourcekolab_plugin.cpp
  LINK
    kcalkolab
)

install(FILES
  kolab.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kcal)


# install-data-local: hide the services of the old imap resource
kdepim_install_uninstall_desktop(../uninstall.desktop
  ${KDE3_SERVICES_INSTALL_DIR}/kresources/kcal/imap.desktop)
