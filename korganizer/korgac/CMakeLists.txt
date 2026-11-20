# before the settings of this directory, which the subdirectories
# do not have in Makefile.am
add_subdirectory(pixmaps)

include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/korganizer"
)

kde3_add_executable(korgac
  SOURCES
    korgacmain.cpp
    alarmdialog.cpp
    alarmdockwindow.cpp
    koalarmclient.cpp
    alarmclientiface.skel
  MOC
    alarmdialog.h
    alarmdockwindow.h
    koalarmclient.h
  LINK
    KDE3::kdeui
    korganizer_eventviewer
)

install(FILES
  korgac.desktop
  DESTINATION ${KDE3_AUTOSTART_INSTALL_DIR})
