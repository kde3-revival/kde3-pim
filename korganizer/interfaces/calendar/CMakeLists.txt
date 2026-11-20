install(FILES
  plugin.h
  calendardecoration.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/calendar)

install(FILES
  calendarplugin.desktop
  calendardecoration.desktop
  DESTINATION ${KDE3_SERVICETYPES_INSTALL_DIR})
