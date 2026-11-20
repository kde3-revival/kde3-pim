include_directories(
  "${PROJECT_SOURCE_DIR}/libksieve"
)

kde3_add_library(ksieve_shared OBJECT
  SOURCES
    error.cpp
  LINK
    KDE3::kdecore
)
