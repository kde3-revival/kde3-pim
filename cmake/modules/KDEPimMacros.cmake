#[=======================================================================[.rst:
KDEPimMacros
------------

Rules of the kdepim Makefile.am files that the KDE3 package of kdelibs
has no function for.

``kdepim_dummy_cpp(<var>)``
  ``dummy.cpp: echo > dummy.cpp``: the empty source a library needs whose
  code is all in convenience libraries.  Sets ``<var>`` to the generated
  file in the current binary directory.
``kdepim_install_uninstall_desktop(<file> <dir>...)``
  ``install-data-local: $(INSTALL_DATA) uninstall.desktop
  $(DESTDIR)<dir>/<name>``: install ``<file>`` (a "Hidden=true"
  .desktop file) in place of the files of an obsolete component, under
  each ``<dir>/<name>`` given as ``<dir>/<name>``.
``kdepim_all_includes(<dir>... SYSTEM <dir>...)``
  ``$(all_includes)``: include directories every target gets after its
  own (those of its directory, the INCLUDES of Makefile.am), appended to
  every target at the end of the top-level directory.
#]=======================================================================]

include_guard(GLOBAL)

function(kdepim_dummy_cpp var)
  set(_f "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp")
  get_property(_done DIRECTORY PROPERTY _KDEPIM_DUMMY_CPP)
  if(NOT _done)
    set_property(DIRECTORY PROPERTY _KDEPIM_DUMMY_CPP TRUE)
    add_custom_command(OUTPUT "${_f}"
      COMMAND ${CMAKE_COMMAND} -E touch "${_f}"
      VERBATIM)
  endif()
  set(${var} "${_f}" PARENT_SCOPE)
endfunction()

function(kdepim_install_uninstall_desktop file)
  get_filename_component(_src "${file}" ABSOLUTE)
  foreach(_dest IN LISTS ARGN)
    get_filename_component(_dir "${_dest}" DIRECTORY)
    get_filename_component(_name "${_dest}" NAME)
    install(FILES "${_src}" DESTINATION "${_dir}" RENAME "${_name}")
  endforeach()
endfunction()

function(_kdepim_collect_targets dir out)
  get_property(_t DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_d IN LISTS _subdirs)
    _kdepim_collect_targets("${_d}" _sub)
    list(APPEND _t ${_sub})
  endforeach()
  set(${out} ${_t} PARENT_SCOPE)
endfunction()

function(_kdepim_apply_all_includes)
  get_property(_dirs GLOBAL PROPERTY _KDEPIM_ALL_INCLUDES)
  get_property(_sysdirs GLOBAL PROPERTY _KDEPIM_ALL_INCLUDES_SYSTEM)
  _kdepim_collect_targets("${CMAKE_SOURCE_DIR}" _targets)
  foreach(_t IN LISTS _targets)
    get_target_property(_type ${_t} TYPE)
    if(_type MATCHES "^(SHARED_LIBRARY|MODULE_LIBRARY|STATIC_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
      target_include_directories(${_t} PRIVATE ${_dirs})
      target_include_directories(${_t} SYSTEM PRIVATE ${_sysdirs})
    endif()
  endforeach()
endfunction()

function(kdepim_all_includes)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "" "" "SYSTEM")
  set_property(GLOBAL PROPERTY _KDEPIM_ALL_INCLUDES ${ARG_UNPARSED_ARGUMENTS})
  set_property(GLOBAL PROPERTY _KDEPIM_ALL_INCLUDES_SYSTEM ${ARG_SYSTEM})
  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL _kdepim_apply_all_includes)
endfunction()
