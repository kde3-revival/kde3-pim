include_directories(
  "${PROJECT_SOURCE_DIR}/kaddressbook/interfaces"
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kabcommon OBJECT
  SOURCES
    kabprefs.cpp
    kabprefs_base.kcfgc
    locationmap.cpp
    filter.cpp
  MOC
    locationmap.h
  LINK
    kabinterfaces
    kdepim
)
