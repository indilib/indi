bin_PROGRAMS = qsidemo

qsidemo_CPPFLAGS = $(all_includes) -I$(top_srcdir)/lib -I/usr/local/include

qsidemo_CXXFLAGS = -g

qsidemo_LDFLAGS = $(all_libraries) -L$(top_srcdir)/lib -L/usr/local/lib  

qsidemo_LDADD = $(top_builddir)/lib/libqsiapi.la -ltiff 

qsidemo_SOURCES = qsidemo.cpp
