include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/../.."
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

kde3_add_module(kio_sieve
  SOURCES
    sieve.cpp
  LINK
    KDE3::kio
    ${SASL2_LIBS}
)

install(FILES
  sieve.protocol
  DESTINATION ${KDE3_SERVICES_INSTALL_DIR})
