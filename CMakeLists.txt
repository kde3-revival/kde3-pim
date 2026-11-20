#    This file is part of the KDE PIM distribution
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.

cmake_minimum_required(VERSION 3.20...4.4)

project(kdepim VERSION 3.5.10 LANGUAGES C CXX)

set(KDEPIM_VERSION "${PROJECT_VERSION}")

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

# kdelibs: the CMake package, install prefix, compiler settings, RUNPATH
include(cmake/KDELibs.cmake)

include(KDEPimMacros)

include(ConfigureChecks.cmake)
configure_file(config.h.cmake "${PROJECT_BINARY_DIR}/config.h")
# automake passes -DHAVE_CONFIG_H (DEFS)
add_compile_definitions(HAVE_CONFIG_H)

# -I$(top_builddir) of every Makefile: config.h
include_directories("${PROJECT_BINARY_DIR}")
# $(all_includes), which every Makefile.am has after its own INCLUDES:
# configure.in.mid's -I$(top_srcdir)/libkdepim, then the KDE and Qt
# headers (for the targets that link no kdelibs library: gpgme++ includes
# kdepimmacros.h); added to every target at the end of the configuration
kdepim_all_includes(
  "${PROJECT_SOURCE_DIR}/libkdepim"
  SYSTEM "${KDE3_INCLUDE_DIR}" "${KDE3Qt_INCLUDE_DIR}")

add_subdirectory(libkdenetwork)
add_subdirectory(libkmime)
add_subdirectory(mimelib)
add_subdirectory(libemailfunctions)
add_subdirectory(ktnef)
add_subdirectory(libkholidays)
add_subdirectory(libkpgp)
add_subdirectory(libksieve)
add_subdirectory(kdgantt)
add_subdirectory(indexlib)
add_subdirectory(libkcal)
add_subdirectory(certmanager)
add_subdirectory(libkdepim)
