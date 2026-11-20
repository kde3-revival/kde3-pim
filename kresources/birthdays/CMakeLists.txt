include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}"
  "${PROJECT_BINARY_DIR}/kaddressbook/common"
  "${PROJECT_SOURCE_DIR}/kaddressbook/common"
)

kde3_add_module(kcal_kabc
  SOURCES
    resourcekabc.cpp
    resourcekabcconfig.cpp
  MOC
    resourcekabc.h
    resourcekabcconfig.h
  LINK
    kcal
    kabcommon
)

install(FILES
  resourcekabc.h
  resourcekabcconfig.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/libkcal)

install(FILES
  kabc.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kcal)


# resourcekabcconfig.lo: ../../kaddressbook/common/kabprefs_base.h
add_dependencies(kcal_kabc kabcommon)
