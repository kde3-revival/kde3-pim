include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

kde3_add_module(libkaddrbk_csv_xxport
  SOURCES
    csv_xxport.cpp
    csvimportdialog.cpp
    dateparser.cpp
  MOC
    csv_xxport.h
    csvimportdialog.h
  LINK
    kabinterfaces
    kdepim
)

kde3_add_module(libkaddrbk_vcard_xxport
  SOURCES
    vcard_xxport.cpp
  MOC
    vcard_xxport.h
  LINK
    kabinterfaces
    kdepim
    kaddressbook
    qgpgme
)

kde3_add_module(libkaddrbk_bookmark_xxport
  SOURCES
    bookmark_xxport.cpp
  MOC
    bookmark_xxport.h
  LINK
    kabinterfaces
    kdepim
)

kde3_add_module(libkaddrbk_eudora_xxport
  SOURCES
    eudora_xxport.cpp
  MOC
    eudora_xxport.h
  LINK
    kabinterfaces
    kdepim
)

kde3_add_module(libkaddrbk_ldif_xxport
  SOURCES
    ldif_xxport.cpp
  MOC
    ldif_xxport.h
  LINK
    kabinterfaces
    kdepim
)

kde3_add_module(libkaddrbk_opera_xxport
  SOURCES
    opera_xxport.cpp
  MOC
    opera_xxport.h
  LINK
    kabinterfaces
    kdepim
)

kde3_add_module(libkaddrbk_pab_xxport
  SOURCES
    pab_xxport.cpp
    pab_mapihd.cpp
    pab_pablib.cpp
  MOC
    pab_xxport.h
  LINK
    kabinterfaces
    kdepim
)

# compile_GNOKIIXXPORT
if(LIB_GNOKII)
kde3_add_module(libkaddrbk_gnokii_xxport
  SOURCES
    gnokii_xxport.cpp
  MOC
    gnokii_xxport.h
  LINK
    X11::Xpm
    ${LIB_GNOKII}
    kabinterfaces
    kdepim
)
install(FILES
  gnokii_xxportui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kaddressbook)
endif()

install(FILES
  csv_xxport.desktop
  vcard_xxport.desktop
  bookmark_xxport.desktop
  eudora_xxport.desktop
  ldif_xxport.desktop
  gnokii_xxport.desktop
  opera_xxport.desktop
  pab_xxport.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kaddressbook)

install(FILES
  csv_xxportui.rc
  vcard_xxportui.rc
  bookmark_xxportui.rc
  eudora_xxportui.rc
  ldif_xxportui.rc
  opera_xxportui.rc
  pab_xxportui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kaddressbook)

