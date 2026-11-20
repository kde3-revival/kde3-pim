include_directories(
  "${PROJECT_SOURCE_DIR}"
)

kde3_add_library(kgroupwarebase SHARED VERSION_INFO 0:0:0
  SOURCES
    folderselectdialog.cpp
  LINK
    KDE3::kdeui
)
