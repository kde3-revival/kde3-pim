include_directories(
  "${PROJECT_BINARY_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/kaddressbook/common"
)

kde3_add_module(kcm_kabconfig
  SOURCES
    kcmkabconfig.cpp
    kabconfigwidget.cpp
    extensionconfigdialog.cpp
    addresseewidget.cpp
  MOC
    addresseewidget.h
    extensionconfigdialog.h
    kabconfigwidget.h
    kcmkabconfig.h
  LINK
    KDE3::kio
    kabinterfaces
    kdepim
    kabcommon
)

kde3_add_module(kcm_kabldapconfig
  SOURCES
    addhostdialog.cpp
    kcmkabldapconfig.cpp
    ldapoptionswidget.cpp
  MOC
    addhostdialog.h
    kcmkabldapconfig.h
    ldapoptionswidget.h
  LINK
    KDE3::kio
    KDE3::kabc
    kdepim
)

kde3_add_module(kcm_kabcustomfields
  SOURCES
    kcmkabcustomfields.cpp
  LINK
    KDE3::kdeui
    KDE3Qt::QUI
    kabcommon
)

install(FILES
  kabconfig.desktop
  kabldapconfig.desktop
  kabcustomfields.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
