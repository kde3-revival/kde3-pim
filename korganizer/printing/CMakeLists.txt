include_directories(
  "${PROJECT_SOURCE_DIR}/korganizer/interfaces"
  "${PROJECT_SOURCE_DIR}/korganizer"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
)

kde3_add_library(kocorehelper SHARED VERSION_INFO 1:0
  SOURCES
    cellitem.cpp
  LINK
    KDE3::kdeui
)

kde3_add_library(korg_stdprinting SHARED VERSION_INFO 1:0
  SOURCES
    calprinter.cpp
    calprintpluginbase.cpp
    calprintdefaultplugins.cpp
    calprintdayconfig_base.ui
    calprintmonthconfig_base.ui
    calprinttodoconfig_base.ui
    calprintweekconfig_base.ui
    calprintincidenceconfig_base.ui
  MOC
    calprinter.h
  LINK
    KDE3::kdeui
    kocorehelper
    kdepim
    kcal
    KDE3::kdeprint
)

install(FILES
  calprintpluginbase.h
  calprinter.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/korganizer)
