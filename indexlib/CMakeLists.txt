# KDE_CXXFLAGS = -include $(srcdir)/compat.h $(USE_EXCEPTIONS)
kde3_add_library(index SHARED VERSION_INFO 0:0:0
  EXCEPTIONS
  SOURCES
    bitstream.cpp
    compressed.cpp
    create.cpp
    exception.cpp
    ifile.cpp
    leafdata.cpp
    leafdatavector.cpp
    lockfile.cpp
    logfile.cpp
    match.cpp
    mmap_manager.cpp
    quotes.cpp
    slow.cpp
    stringarray.cpp
    stringset.cpp
    tokenizer.cpp
  # libindex.la had no LIBADD and was linked without -no-undefined:
  # compressed.cpp's zlib calls were resolved by the programs using it
  LINK_PRIVATE
    ZLIB::ZLIB
)
target_compile_options(index PRIVATE
  "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/compat.h")

install(FILES
  create.h
  index.h
  lockfile.h
  DESTINATION ${KDE3_INCLUDE_INSTALL_DIR}/index)

# AC_OUTPUT(indexlib/indexlib-config) of configure.in.in; autoconf
# substituted exec_prefix, libdir and includedir unexpanded
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${KDE3_LIB_INSTALL_DIR}")
set(includedir "\${prefix}/${KDE3_INCLUDE_INSTALL_DIR}")
configure_file(indexlib-config.in indexlib-config @ONLY)

install(PROGRAMS
  "${CMAKE_CURRENT_BINARY_DIR}/indexlib-config"
  DESTINATION ${KDE3_BIN_INSTALL_DIR})
