include_directories(
  "${PROJECT_SOURCE_DIR}/libksieve"
)

kde3_add_library(ksieve_parser OBJECT
  SOURCES
    utf8validator.cpp
    lexer.cpp
    parser.cpp
  LINK
    ksieve_shared
)
