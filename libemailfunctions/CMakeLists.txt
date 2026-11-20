# INCLUDES = $(all_includes) -I$(srcdir)/../libkmime/
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/../libkmime/"
)

kde3_add_library(emailfunctions OBJECT
  SOURCES
    email.cpp
    idmapper.cpp
    kasciistricmp.cpp
    kasciistringtools.cpp
    networkstatus.cpp
    networkstatus.skel
  MOC
    networkstatus.h
  LINK
    kmime
    KDE3Qt::Qt
    KDE3::kdecore
)

install(FILES
  idmapper.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/libemailfunctions)
