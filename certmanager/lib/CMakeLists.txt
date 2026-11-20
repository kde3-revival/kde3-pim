add_subdirectory(pics)
add_subdirectory(kleo)
add_subdirectory(backends)
add_subdirectory(ui)

include_directories(
  "${PROJECT_SOURCE_DIR}/libkdenetwork"
)

kde3_add_library(kleopatra SHARED VERSION_INFO 1:0:0
  SOURCES
    cryptplug.cpp
    cryptplugwrapper.cpp
    cryptplugwrapperlist.cpp
    cryptplugfactory.cpp
  MOC
    cryptplugfactory.h
  LINK
    kleopatra_core
    kleopatra_backend_qgpgme
    kleopatra_ui
    qgpgme
    KDE3::kio
    KDE3::kdeui
)
# Disabled for kdepim-3.3, see also kleo/cryptobackendfactory.cpp
# backends/kpgp/libkleopatra_backend_kpgp.la

# install-data-local: without the .desktop suffix (which is needed for
# translation)
install(FILES
  libkleopatrarc.desktop
  DESTINATION ${KDE3_CONFIG_INSTALL_DIR}
  RENAME libkleopatrarc)
