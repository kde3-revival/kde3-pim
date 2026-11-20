# install-data-local: without the .desktop suffix (which is needed for
# translation)
foreach(_profile default html high-contrast purist secure)
  install(FILES
    profile-${_profile}-rc.desktop
    DESTINATION ${KDE3_DATA_INSTALL_DIR}/kmail
    RENAME profile-${_profile}-rc)
endforeach()
