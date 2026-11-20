# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(holidays)

include_directories(
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kholidays SHARED VERSION_INFO 1:0:0
  SOURCES
    kholidays.cpp
    scanholiday.c
    parseholiday.c
    lunarphase.cpp
  LINK
    KDE3::kio
)
