include_directories(
  "${PROJECT_SOURCE_DIR}/kmail"
  "${PROJECT_SOURCE_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}/libkmime"
  "${PROJECT_SOURCE_DIR}/mimelib"
  "${PROJECT_SOURCE_DIR}/libemailfunctions"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_BINARY_DIR}/libkdepim"
  "${PROJECT_SOURCE_DIR}/korganizer"
)

# KDE_CXXFLAGS = $(USE_RTTI): -frtti is the default

kde3_add_module(libkmail_bodypartformatter_text_vcard
  SOURCES
    text_vcard.cpp
  LINK
    KDE3Qt::Qt
    KDE3::kabc
    kdepim
    kmime
    emailfunctions
)

kde3_add_module(libkmail_bodypartformatter_text_calendar
  SOURCES
    text_calendar.cpp
    delegateselector.cpp
    attendeeselector.cpp
    ui_attendeeselector.ui
    ../../../korganizer/kcalendariface.stub
  MOC
    attendeeselector.h
    delegateselector.h
  LINK
    KDE3Qt::Qt
    KDE3::kabc
    kcal
    kmailprivate
)

kde3_add_module(libkmail_bodypartformatter_text_xdiff
  SOURCES
    text_xdiff.cpp
  LINK
    KDE3Qt::Qt
)

install(FILES
  text_vcard.desktop
  text_calendar.desktop
  text_xdiff.desktop
  DESTINATION ${KDE3_DATA_INSTALL_DIR}/kmail/plugins/bodypartformatter)

