bin_PROGRAMS = qsidemo \
	ExternalTrigger

qsidemo_CPPFLAGS = $(all_includes) -I$(top_srcdir)/lib -I/usr/local/include

qsidemo_CXXFLAGS = -g

qsidemo_LDFLAGS = $(all_libraries) -L$(top_srcdir)/lib -L/usr/local/lib  

qsidemo_LDADD = $(top_builddir)/lib/libqsiapi.la -ltiff 

qsidemo_SOURCES = qsidemo.cpp

ExternalTrigger_CPPFLAGS = $(all_includes) -I$(top_srcdir)/lib -I/usr/local/include

ExternalTrigger_CXXFLAGS = -g

ExternalTrigger_LDFLAGS = $(all_libraries) -L$(top_srcdir)/lib -L/usr/local/lib  

ExternalTrigger_LDADD = $(top_builddir)/lib/libqsiapi.la -ltiff 

ExternalTrigger_SOURCES =  \
	ExternalTrigger.cpp
