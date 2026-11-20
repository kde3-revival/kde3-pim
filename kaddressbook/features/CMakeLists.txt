include_directories(
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}"
)

# compile_newdistrlists (KDEPIM_NEW_DISTRLISTS) builds the new widget;
# the old one is then a module without sources, which is left out here
if(KDEPIM_NEW_DISTRLISTS)
kde3_add_module(libkaddrbk_distributionlistng
  SOURCES
    distributionlistngwidget.cpp
  MOC
    distributionlistngwidget.h
  LINK
    kabinterfaces
    kdepim
)
else()
kde3_add_module(libkaddrbk_distributionlist
  SOURCES
    distributionlistwidget.cpp
  MOC
    distributionlistwidget.h
  LINK
    kabinterfaces
    kdepim
)
endif()

kde3_add_module(libkaddrbk_resourceselection
  SOURCES
    resourceselection.cpp
  MOC
    resourceselection.h
  LINK
    kabinterfaces
    kdepim
)

if(KDEPIM_NEW_DISTRLISTS)
  set(_distributionlist_desktop distributionlistng.desktop)
else()
  set(_distributionlist_desktop distributionlist.desktop)
endif()
install(FILES
  ${_distributionlist_desktop}
  resourceselection.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kaddressbook)

