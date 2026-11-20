include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/kresources/lib"
)

kde3_use_dcopidlng(../../../kmail/kmailicalIface.h)

kde3_add_library(resourcekolabshared OBJECT
  SOURCES
    resourcekolabbase.cpp
    kmailconnection.cpp
    kolabbase.cpp
    subresource.cpp
    kmailconnection.skel
    ../../../kmail/kmailicalIface.stub
  MOC
    kmailconnection.h
  LINK
    kcal
    kdepim
    kgroupwarebase
)
