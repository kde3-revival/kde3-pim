set(DESIGNDATA "${CMAKE_CURRENT_SOURCE_DIR}/../../design-data")
set(ICALSCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts")
set(_bin "${CMAKE_CURRENT_BINARY_DIR}")
# the mk*.pl scripts walk some hashes unsorted ("keys %h"): a fixed hash
# order for reproducible sources
set(ICAL_PERL "${CMAKE_COMMAND}" -E env PERL_HASH_SEED=0 PERL_PERTURB_KEYS=0
  "${PERL_EXECUTABLE}")

include_directories(
  "${PROJECT_BINARY_DIR}/libkcal/libical"
  "${PROJECT_SOURCE_DIR}/libkcal/libical/src"
  "${PROJECT_BINARY_DIR}/libkcal/libical/src"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_compile_definitions(PACKAGE_DATA_DIR="${KDE3_FULL_DATA_INSTALL_DIR}/libical")

# parameters
set(PARAMETERDEPS
  "${ICALSCRIPTS}/mkderivedparameters.pl"
  "${DESIGNDATA}/parameters.csv"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedparameter.c.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedparameter.h.in")
kde3_generate_stdout("${_bin}/icalderivedparameter.h"
  COMMAND ${ICAL_PERL} -I "${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedparameters.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedparameter.h.in"
          -h "${DESIGNDATA}/parameters.csv"
  DEPENDS ${PARAMETERDEPS})
kde3_generate_stdout("${_bin}/icalderivedparameter.c"
  COMMAND ${ICAL_PERL} -I "${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedparameters.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedparameter.c.in"
          -c "${DESIGNDATA}/parameters.csv"
  DEPENDS ${PARAMETERDEPS} "${CMAKE_CURRENT_SOURCE_DIR}/icalparameter.h")

# properties
set(PROPERTYDEPS
  "${ICALSCRIPTS}/mkderivedproperties.pl"
  "${DESIGNDATA}/properties.csv"
  "${DESIGNDATA}/value-types.csv"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedproperty.c.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedproperty.h.in")
kde3_generate_stdout("${_bin}/icalderivedproperty.h"
  COMMAND ${ICAL_PERL} "-I${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedproperties.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedproperty.h.in"
          -h "${DESIGNDATA}/properties.csv" "${DESIGNDATA}/value-types.csv"
  DEPENDS ${PROPERTYDEPS})
kde3_generate_stdout("${_bin}/icalderivedproperty.c"
  COMMAND ${ICAL_PERL} "-I${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedproperties.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedproperty.c.in"
          -c "${DESIGNDATA}/properties.csv" "${DESIGNDATA}/value-types.csv"
  DEPENDS ${PROPERTYDEPS} "${CMAKE_CURRENT_SOURCE_DIR}/icalproperty.h")

# restrictions
kde3_generate_stdout("${_bin}/icalrestriction.c"
  COMMAND ${ICAL_PERL} "${ICALSCRIPTS}/mkrestrictiontable.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalrestriction.c.in"
          "${DESIGNDATA}/restrictions.csv"
  DEPENDS "${ICALSCRIPTS}/mkrestrictiontable.pl"
          "${DESIGNDATA}/restrictions.csv"
          "${CMAKE_CURRENT_SOURCE_DIR}/icalrestriction.c.in")

# values
set(VALUEDEPS
  "${ICALSCRIPTS}/mkderivedvalues.pl"
  "${DESIGNDATA}/value-types.csv"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedvalue.c.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedvalue.h.in")
kde3_generate_stdout("${_bin}/icalderivedvalue.h"
  COMMAND ${ICAL_PERL} "-I${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedvalues.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedvalue.h.in"
          -h "${DESIGNDATA}/value-types.csv"
  DEPENDS ${VALUEDEPS})
kde3_generate_stdout("${_bin}/icalderivedvalue.c"
  COMMAND ${ICAL_PERL} "-I${ICALSCRIPTS}" "${ICALSCRIPTS}/mkderivedvalues.pl"
          -i "${CMAKE_CURRENT_SOURCE_DIR}/icalderivedvalue.c.in"
          -c "${DESIGNDATA}/value-types.csv"
  DEPENDS ${VALUEDEPS} "${_bin}/icalderivedvalue.h")

# ORDERING OF HEADERS IS SIGNIFICANT. Don't change this ordering. It
# is required to make the combined header ical.h properly
set(COMBINEDHEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/icalversion.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icaltime.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalduration.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalperiod.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalenums.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icaltypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalrecur.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalattach.h"
  "${_bin}/icalderivedvalue.h"
  "${_bin}/icalderivedparameter.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalvalue.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalparameter.h"
  "${_bin}/icalderivedproperty.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalproperty.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/pvl.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalarray.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalcomponent.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icaltimezone.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalparser.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalmemory.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalerror.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalrestriction.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/sspm.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/icalmime.h")
kde3_generate_stdout("${_bin}/ical.h"
  COMMAND "${PERL_EXECUTABLE}"
          "${PROJECT_SOURCE_DIR}/cmake/modules/kdepim_combine_headers.pl"
          ${COMBINEDHEADERS}
  DEPENDS ${COMBINEDHEADERS}
          "${PROJECT_SOURCE_DIR}/cmake/modules/kdepim_combine_headers.pl")

# Order is important!
kde3_add_library(ical OBJECT
  SOURCES
    caldate.c
    icalarray.c
    icalattach.c
    "${_bin}/icalderivedparameter.c"
    icalrecur.c
    "${_bin}/icalrestriction.c"
    icalcomponent.c
    icalenums.c
    icalerror.c
    icalmemory.c
    icalmime.c
    icalparameter.c
    icalparser.c
    "${_bin}/icalderivedproperty.c"
    icalproperty.c
    icaltime.c
    icalduration.c
    icalperiod.c
    icaltimezone.c
    icaltypes.c
    icalvalue.c
    "${_bin}/icalderivedvalue.c"
    pvl.c
    sspm.c
    vsnprintf.c
    # the derived headers (made before the objects, as the icalcomponent.lo
    # etc. rules said) and ical.h for libkcal (libkcal_la_COMPILE_FIRST)
    "${_bin}/icalderivedvalue.h"
    "${_bin}/icalderivedproperty.h"
    "${_bin}/icalderivedparameter.h"
    "${_bin}/ical.h"
)
