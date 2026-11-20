include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/../libical"
  "${CMAKE_CURRENT_BINARY_DIR}/../libical"
)

# "We don't need icalss.h, but it needs to be built..."
# (libicalss_la_COMPILE_FIRST = ../libical/ical.h icalss.h)
kde3_generate_stdout("${CMAKE_CURRENT_BINARY_DIR}/icalss.h"
  COMMAND "${PERL_EXECUTABLE}"
          "${PROJECT_SOURCE_DIR}/cmake/modules/kdepim_combine_headers.pl"
          --keep-config
          "${CMAKE_CURRENT_SOURCE_DIR}/icalclassify.h"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icalclassify.h"
          "${PROJECT_SOURCE_DIR}/cmake/modules/kdepim_combine_headers.pl")

kde3_add_library(icalss OBJECT
  SOURCES
    icalclassify.c
    "${CMAKE_CURRENT_BINARY_DIR}/icalss.h"
)
# icalclassify.c includes ical.h, made in ../libical
add_dependencies(icalss ical)
