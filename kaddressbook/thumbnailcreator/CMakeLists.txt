kde3_add_module(ldifvcardthumbnail
  SOURCES
    ldifvcardcreator.cpp
  LINK
    KDE3::kdecore
    KDE3::kabc
    KDE3::kdefx
)

install(FILES
  ldifvcardthumbnail.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
