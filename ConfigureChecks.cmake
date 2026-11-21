# Configure checks for config.h, following configure.in.in and the
# subdirectories' configure.in.in.  Only what the sources actually test
# is checked.

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

find_package(PkgConfig REQUIRED)

# ---- headers (configure.in.in) -------------------------------------------
foreach(_h
    byteswap.h err.h fcntl.h inttypes.h paths.h stdint.h sys/cdefs.h
    sys/stat.h sys/types.h)
  string(TOUPPER "HAVE_${_h}" _var)
  string(REGEX REPLACE "[/.]" "_" _var "${_var}")
  check_include_file("${_h}" ${_var})
endforeach()

# ---- functions -------------------------------------------------------------
# libical/src/libical/vsnprintf.c is only compiled in without snprintf()
check_function_exists(snprintf HAVE_SNPRINTF)

# AC_C_BIGENDIAN (kmail)
if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(WORDS_BIGENDIAN 1)
endif()

# ---- libkmime (configure.in.in, libkmime/configure.in.in) ----------------
check_c_source_compiles("#include <time.h>
int main(void) { timezone = 1; return 0; }" HAVE_TIMEZONE)
check_c_source_compiles("#include <time.h>
int main(void) { struct tm tm; tm.tm_gmtoff = 1; return (int)tm.tm_gmtoff; }"
  HAVE_TM_GMTOFF)

# stl coping with namespace std (configure.in.in)
check_cxx_source_compiles("#include <iterator>
struct s : public std::iterator<std::forward_iterator_tag, int> {};
int main() { return 0; }" HAVE_STLNAMESPACE)
if(HAVE_STLNAMESPACE)
  set(STD_NAMESPACE_PREFIX "std::")
else()
  set(STD_NAMESPACE_PREFIX "")
endif()

# ---- ktnef (ktnef/configure.in.in) -----------------------------------------
check_type_size("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
check_type_size(uint64_t SIZEOF_UINT64_T)

# ---- certmanager (certmanager/configure.in.in) -----------------------------
# MAX_CMD_LENGTH = libtool's lt_cv_sys_max_cmd_len: half the longest
# argument "echo" took in its test, which is 32768 on Linux (an argument
# of 64 KiB passes, 128 KiB is MAX_ARG_STRLEN) and the BSDs.
set(MAX_CMD_LENGTH 32768 CACHE STRING
  "Maximum length of command line arguments (libtool's max_cmd_len)")
mark_as_advanced(MAX_CMD_LENGTH)

# ---- gpgme (libkdenetwork/gpgmepp/configure.in.in) ---------------------------
# gpgme >= 0.4.5; gpgme 2 has no gpgme-config any more, but a pkg-config
# file.  GPGME_LIBS had -lgpg-error added when gpgme-config left it out.
pkg_check_modules(GPGME REQUIRED IMPORTED_TARGET "gpgme>=0.4.5" gpg-error)
set(HAVE_GPGME_0_4_BRANCH 1)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${GPGME_INCLUDE_DIRS})
set(CMAKE_REQUIRED_FLAGS ${GPGME_CFLAGS_OTHER})
# the libraries after the test's object (link options would come before
# it, which --as-needed drops)
set(CMAKE_REQUIRED_LIBRARIES PkgConfig::GPGME)
set(CMAKE_REQUIRED_QUIET OFF)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_keylist_mode_t mode = GPGME_KEYLIST_MODE_VALIDATE; return (int)mode; }"
  HAVE_GPGME_KEYLIST_MODE_VALIDATE)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_ctx_t ctx = 0; gpgme_error_t e = gpgme_cancel(ctx); return (int)e; }"
  HAVE_GPGME_CANCEL)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_key_t key = 0; key->keylist_mode = 0; return 0; }"
  HAVE_GPGME_KEY_T_KEYLIST_MODE)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_decrypt_result_t res = 0; unsigned int wku = res->wrong_key_usage; return (int)wku; }"
  HAVE_GPGME_WRONG_KEY_USAGE)
check_c_source_compiles("#include <gpgme.h>
int main(void) { int i = GPGME_INCLUDE_CERTS_DEFAULT; return i; }"
  HAVE_GPGME_INCLUDE_CERTS_DEFAULT)
# gpgme 2 has no trust item listing any more (GnuPG 2 had none either)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_ctx_t ctx = 0; gpgme_error_t e = gpgme_op_trustlist_start(ctx, 0, 0); return (int)e; }"
  HAVE_GPGME_OP_TRUSTLIST)
check_c_source_compiles("#include <gpgme.h>
int main(void) { gpgme_ctx_t ctx = 0; gpgme_data_t data = 0; unsigned int flags = 0;
  gpgme_error_t e = gpgme_op_getauditlog(ctx, data, flags); return (int)e; }"
  HAVE_GPGME_OP_GETAUDITLOG)
cmake_pop_check_state()

# ---- libkdepim (libkdepim/configure.in.in) ----------------------------------
option(KDEPIM_NEW_DISTRLISTS
  "New distribution lists, saved as contacts in the address book (useful for Kolab)" ON)

# ---- kioslaves (kioslaves/configure.in.in): cyrus-sasl 2 -------------------
set(KDEPIM_WITH_SASL "" CACHE STRING
  "cyrus-sasl 2: empty = use it if found, OFF = do not use it, ON = require it")
set(SASL2_LIBS "")
if(KDEPIM_WITH_SASL STREQUAL "" OR KDEPIM_WITH_SASL)
  check_include_file(sasl/sasl.h _have_sasl_sasl_h)
  if(_have_sasl_sasl_h)
    check_library_exists(sasl2 sasl_client_init "" _have_libsasl2)
    if(_have_libsasl2)
      set(SASL2_LIBS sasl2)
      set(HAVE_LIBSASL2 1)
    endif()
  endif()
  if(KDEPIM_WITH_SASL AND NOT HAVE_LIBSASL2)
    message(FATAL_ERROR "KDEPIM_WITH_SASL was given, but test for cyrus-sasl failed")
  endif()
endif()

# kioslaves/sieve/configure.in.in (--enable-broken-timsieved-workaround)
option(KDEPIM_BROKEN_TIMSIEVED_WORKAROUND
  "Make the sieve ioslave talk to timsieved <= 1.1.0 (omit HAVESPACE)" OFF)
if(KDEPIM_BROKEN_TIMSIEVED_WORKAROUND)
  set(HAVE_BROKEN_TIMSIEVED 1)
endif()

# ---- kmail (kmail/configure.in.in) -------------------------------------------
# --enable-indexlib: EXPERIMENTAL full-text indexing in KMail
option(KDEPIM_INDEXLIB "Enable EXPERIMENTAL full-text indexing in KMail" OFF)
if(KDEPIM_INDEXLIB)
  set(HAVE_INDEXLIB 1)
endif()

# --disable-inotify: Linux inode notifications
option(KDEPIM_WITH_INOTIFY "Use Linux inode notifications (KMail's editor watcher)" ON)
if(KDEPIM_WITH_INOTIFY)
  # configure.in.in tested <linux/inotify.h> and __NR_inotify_init, which
  # EditorWatcher called through syscall(); it now uses the C library's
  # <sys/inotify.h> functions (__NR_inotify_init does not exist on newer
  # architectures such as aarch64 and riscv64, which have only
  # inotify_init1, wrapped by the C library's inotify_init())
  check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
endif()

# ---- indexlib (indexlib/configure.in.in) -------------------------------------
# -DHAVE_BOOST in the CXXFLAGS of everything when boost/format.hpp is found
check_include_file_cxx(boost/format.hpp _have_boost_format_hpp)
if(_have_boost_format_hpp)
  add_compile_definitions("$<$<COMPILE_LANGUAGE:CXX>:HAVE_BOOST>")
endif()

# ---- kaddressbook/xxport (configure.in.in): gnokii -----------------------------
set(KDEPIM_WITH_GNOKII "" CACHE STRING
  "Mobile phones through gnokii: empty = use it if found, OFF = do not use it, ON = require it")
set(LIB_GNOKII "")
if(KDEPIM_WITH_GNOKII STREQUAL "" OR KDEPIM_WITH_GNOKII)
  pkg_check_modules(GNOKII QUIET IMPORTED_TARGET gnokii)
  if(GNOKII_FOUND)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES ${GNOKII_INCLUDE_DIRS})
    check_include_file(gnokii.h HAVE_GNOKII_H)
    cmake_pop_check_state()
    if(HAVE_GNOKII_H)
      set(LIB_GNOKII PkgConfig::GNOKII)
    endif()
  endif()
  if(KDEPIM_WITH_GNOKII AND NOT LIB_GNOKII)
    message(FATAL_ERROR "KDEPIM_WITH_GNOKII was given, but test for gnokii failed")
  endif()
endif()

# ---- kmail: suspend/resume through systemd-logind -------------------------------
# KMail listens to logind's PrepareForSleep signal with GDBus (logindsleep.cpp);
# the callbacks need Qt running on the GLib event loop (QT_GLIB_EVENTLOOP).
set(KDEPIM_WITH_LOGIND "" CACHE STRING
  "KMail disconnects before suspend (logind, GIO): empty = use it if found, OFF = do not use it, ON = require it")
set(LIB_GIO_UNIX "")
if(KDEPIM_WITH_LOGIND STREQUAL "" OR KDEPIM_WITH_LOGIND)
  pkg_check_modules(GIO_UNIX QUIET IMPORTED_TARGET gio-unix-2.0)
  if(GIO_UNIX_FOUND)
    set(LIB_GIO_UNIX PkgConfig::GIO_UNIX)
    set(HAVE_GIO_UNIX 1)
  endif()
  if(KDEPIM_WITH_LOGIND AND NOT LIB_GIO_UNIX)
    message(FATAL_ERROR "KDEPIM_WITH_LOGIND was given, but gio-unix-2.0 was not found")
  endif()
endif()

# ---- libkcal/libical (libkcal/libical/configure.in.in) --------------------------
find_program(PERL_EXECUTABLE NAMES perl5 perl)
if(NOT PERL_EXECUTABLE)
  message(FATAL_ERROR "perl is needed to generate the sources of libical")
endif()
set(ICAL_UNIX_NEWLINE 1)

# _FILE_OFFSET_BITS (KDE_CHECK_LARGEFILE)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(_FILE_OFFSET_BITS 64)
endif()

# configure.in.bot of kioslaves, kmail and kaddressbook/xxport: the
# summary of what is left out.  <result variable>:<option>:<what>
foreach(_what
    "HAVE_LIBSASL2:KDEPIM_WITH_SASL:cyrus-sasl 2 (the sieve ioslave is not built, imap4 lacks a lot of authentication methods)"
    "LIB_GNOKII:KDEPIM_WITH_GNOKII:libgnokii (the KDE Addressbook mobile phone import/export filter)"
    "HAVE_INDEXLIB:KDEPIM_INDEXLIB:KMail's experimental full-text indexing"
    "HAVE_INOTIFY:KDEPIM_WITH_INOTIFY:Linux inode notifications"
    "LIB_GIO_UNIX:KDEPIM_WITH_LOGIND:gio-unix-2.0 (KMail does not disconnect before suspend)")
  string(REPLACE ":" ";" _what "${_what}")
  list(GET _what 0 _var)
  list(GET _what 1 _opt)
  list(GET _what 2 _desc)
  if(NOT ${_var})
    if(DEFINED ${_opt} AND NOT "${${_opt}}" STREQUAL "" AND NOT ${_opt})
      message(STATUS "Switched off (${_opt}): ${_desc}")
    else()
      message(STATUS "Not found, built without: ${_desc}")
    endif()
  endif()
endforeach()
