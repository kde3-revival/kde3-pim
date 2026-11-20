set(zoneinfodir ${KDE3_DATA_INSTALL_DIR}/libical/zoneinfo)

install(FILES
  zones.tab
  DESTINATION ${zoneinfodir})

# install-data-local: the *.ics files of $(DIRS)
foreach(_dir
    Africa
    America
    America/Argentina
    America/Indiana
    America/Kentucky
    America/North_Dakota
    Antarctica
    Arctic
    Asia
    Atlantic
    Australia
    Europe
    Indian
    Pacific)
  file(GLOB _ics CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/*.ics")
  install(FILES ${_ics} DESTINATION ${zoneinfodir}/${_dir})
endforeach()
