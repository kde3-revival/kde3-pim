include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
)

kde3_add_module(libkaddrbk_cardview
  SOURCES
    cardview.cpp
    colorlistbox.cpp
    configurecardviewdialog.cpp
    kaddressbookcardview.cpp
  MOC
    cardview.h
    colorlistbox.h
    configurecardviewdialog.h
    kaddressbookcardview.h
  LINK
    kaddressbook
)

kde3_add_module(libkaddrbk_iconview
  SOURCES
    kaddressbookiconview.cpp
  MOC
    kaddressbookiconview.h
  LINK
    kaddressbook
)

kde3_add_module(libkaddrbk_tableview
  SOURCES
    configuretableviewdialog.cpp
    contactlistview.cpp
    kaddressbooktableview.cpp
  MOC
    configuretableviewdialog.h
    contactlistview.h
    kaddressbooktableview.h
  LINK
    kaddressbook
)

install(FILES
  cardview.desktop
  iconview.desktop
  tableview.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kaddressbook)


# kaddressbookiconview.lo kaddressbooktableview.lo kaddressbookcardview.lo:
# ../common/kabprefs_base.h
add_dependencies(libkaddrbk_cardview kabcommon)
add_dependencies(libkaddrbk_iconview kabcommon)
add_dependencies(libkaddrbk_tableview kabcommon)
