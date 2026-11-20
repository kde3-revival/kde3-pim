# before the settings of this directory, which the subdirectories do not
# have in Makefile.am
add_subdirectory(mimelib)

# AM_CPPFLAGS = -D$(LIBVERSION) -D_REENTRANT, LIBVERSION = DW_DEVELOPMENT_VERSION
add_compile_definitions(DW_DEVELOPMENT_VERSION _REENTRANT)

kde3_add_library(mimelib SHARED VERSION_INFO 1:1
  SOURCES
    protocol.cpp
    address.cpp
    addrlist.cpp
    body.cpp
    bodypart.cpp
    boyermor.cpp
    datetime.cpp
    disptype.cpp
    dw_cte.cpp
    dw_date.cpp
    dw_mime.cpp
    entity.cpp
    field.cpp
    fieldbdy.cpp
    group.cpp
    headers.cpp
    mailbox.cpp
    mboxlist.cpp
    mechansm.cpp
    mediatyp.cpp
    message.cpp
    msgcmp.cpp
    msgid.cpp
    nntp.cpp
    param.cpp
    pop.cpp
    dwstring.cpp
    text.cpp
    token.cpp
    uuencode.cpp
    binhex.cpp
)
