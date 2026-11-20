# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(libical)
add_subdirectory(versit)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/libical/src/libical"
  "${CMAKE_CURRENT_SOURCE_DIR}/libical/src/libicalss"
  "${CMAKE_CURRENT_BINARY_DIR}/libical/src/libical"
  "${CMAKE_CURRENT_BINARY_DIR}/libical/src/libicalss"
  "${CMAKE_CURRENT_SOURCE_DIR}/versit"
  "${PROJECT_SOURCE_DIR}/"
  "${PROJECT_SOURCE_DIR}/ktnef"
  "${PROJECT_SOURCE_DIR}/libemailfunctions"
)

kde3_add_library(kcal SHARED VERSION_INFO 2:0:0
  SOURCES
    incidencebase.cpp
    incidence.cpp
    journal.cpp
    todo.cpp
    event.cpp
    freebusy.cpp
    attendee.cpp
    attachment.cpp
    recurrencerule.cpp
    recurrence.cpp
    alarm.cpp
    customproperties.cpp
    calendar.cpp
    calendarlocal.cpp
    calformat.cpp
    vcalformat.cpp
    icalformat.cpp
    icalformatimpl.cpp
    incidenceformatter.cpp
    vcaldrag.cpp
    icaldrag.cpp
    exceptions.cpp
    scheduler.cpp
    imipscheduler.cpp
    dummyscheduler.cpp
    calfilter.cpp
    person.cpp
    period.cpp
    duration.cpp
    dndfactory.cpp
    filestorage.cpp
    compat.cpp
    resourcecalendar.cpp
    resourcelocal.cpp
    resourcelocalconfig.cpp
    resourcelocaldir.cpp
    resourcelocaldirconfig.cpp
    resourcecached.cpp
    resourcecachedconfig.cpp
    calendarresources.cpp
    qtopiaformat.cpp
    htmlexportsettings.kcfgc
    htmlexport.cpp
    calendarnull.cpp
    freebusyurlstore.cpp
    confirmsavedialog.cpp
  MOC
    calendar.h
    calendarresources.h
    resourcecached.h
    resourcecachedconfig.h
    resourcecalendar.h
    resourcelocal.h
    resourcelocalconfig.h
    resourcelocaldir.h
    resourcelocaldirconfig.h
  LINK
    versit
    KDE3::kio
    ical
    icalss
    ktnef
    emailfunctions
    KDE3::kresources
    KDE3::kabc
)

kde3_add_module(kcal_local
  SOURCES
    resourcelocal_plugin.cpp
  LINK
    kcal
)

kde3_add_module(kcal_localdir
  SOURCES
    resourcelocaldir_plugin.cpp
  LINK
    kcal
)

install(FILES
  alarm.h
  attachment.h
  attendee.h
  calendar.h
  calendarlocal.h
  calendarnull.h
  calendarresources.h
  calfilter.h
  calformat.h
  calstorage.h
  customproperties.h
  dndfactory.h
  duration.h
  event.h
  exceptions.h
  filestorage.h
  freebusy.h
  "${CMAKE_CURRENT_BINARY_DIR}/htmlexportsettings.h"
  htmlexport.h
  icaldrag.h
  icalformat.h
  incidencebase.h
  incidence.h
  incidenceformatter.h
  journal.h
  kcalversion.h
  listbase.h
  period.h
  person.h
  qtopiaformat.h
  recurrencerule.h
  recurrence.h
  resourcecached.h
  resourcecalendar.h
  resourcelocalconfig.h
  resourcelocaldirconfig.h
  resourcelocaldir.h
  resourcelocal.h
  scheduler.h
  libkcal_export.h
  todo.h
  vcaldrag.h
  vcalformat.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/libkcal)

install(FILES
  local.desktop
  localdir.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources/kcal)

install(FILES
  kcal_manager.desktop
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR}/kresources)

# convertqtopia is an EXTRA_PROGRAM, not built
