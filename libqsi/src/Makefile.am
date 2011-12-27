bin_PROGRAMS = qsiapitest

# set the include path found by configure

qsiapitest_CPPFLAGS = $(all_includes) -I$(top_srcdir)/lib -I/usr/local/include

qsiapitest_CXXFLAGS = -g

# the library search path.
qsiapitest_LDFLAGS = $(all_libraries) -L$(top_srcdir)/lib -L/usr/local/lib

qsiapitest_LDADD = $(top_builddir)/lib/libqsiapi.la -ltiff 

qsiapitest_SOURCES = qsiapitest.cpp
 