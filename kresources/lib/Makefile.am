INCLUDES = -I$(top_srcdir) $(all_includes)

lib_LTLIBRARIES = libkgroupwarebase.la

libkgroupwarebase_la_SOURCES = folderselectdialog.cpp
libkgroupwarebase_la_LDFLAGS = $(all_libraries) -no-undefined
libkgroupwarebase_la_LIBADD  = $(LIB_KDEUI)

METASOURCES = AUTO

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kdepimresources.pot
