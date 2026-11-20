add_subdirectory(imap4)
add_subdirectory(mbox)
# compile_kio_sieve: with cyrus-sasl 2
if(HAVE_LIBSASL2)
  add_subdirectory(sieve)
endif()
