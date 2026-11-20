include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/kaddressbook"
  "${PROJECT_SOURCE_DIR}/kaddressbook/details"
  "${PROJECT_SOURCE_DIR}/kaddressbook/common"
  "${PROJECT_BINARY_DIR}/kaddressbook/common"
)

kde3_add_library(printer OBJECT
  SOURCES
    detailledstyle.cpp
    ds_appearance.ui
    kabentrypainter.cpp
    mikesstyle.cpp
    printingwizard.cpp
    printprogress.cpp
    printstyle.cpp
    selectionpage.cpp
    stylepage.cpp
    printsortmode.cpp
  MOC
    detailledstyle.h
    mikesstyle.h
    printingwizard.h
    printprogress.h
    printstyle.h
    selectionpage.h
    stylepage.h
  LINK
    KDE3Qt::Qt
)

install(FILES
  detailed-style.png
  mike-style.png
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kaddressbook/printing)


# libprinter_la_COMPILE_FIRST = $(top_builddir)/kaddressbook/common/kabprefs_base.h
add_dependencies(printer kabcommon)
