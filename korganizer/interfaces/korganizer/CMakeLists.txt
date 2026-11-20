install(FILES
  part.h
  baseview.h
  calendarviewbase.h
  mainwindow.h
  corehelper.h
  printplugin.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/korganizer)

install(FILES
  korganizerpart.desktop
  korgprintplugin.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
