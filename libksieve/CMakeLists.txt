# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(ksieve)
add_subdirectory(shared)
add_subdirectory(parser)

kdepim_dummy_cpp(_dummy)
kde3_add_library(ksieve SHARED VERSION_INFO 0:0:0
  SOURCES
    ${_dummy}
  LINK
    ksieve_parser
)
