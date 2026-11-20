include_directories(
  "${PROJECT_SOURCE_DIR}/certmanager/lib"
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

add_compile_definitions(LIBKLEOPATRA_NO_COMPAT)

kde3_add_executable(kwatchgnupg
  SOURCES
    kwatchgnupgmainwin.cpp
    kwatchgnupgconfig.cpp
    aboutdata.cpp
    tray.cpp
    main.cpp
  MOC
    kwatchgnupgconfig.h
    kwatchgnupgmainwin.h
    tray.h
  LINK
    kleopatra_conf
    kleopatra
    KDE3::kutils
    KDE3::kabc
)

install(FILES
  kwatchgnupgui.rc
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kwatchgnupg)

install(FILES
  kwatchgnupg.png
  kwatchgnupg2.png
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kwatchgnupg/pics)
