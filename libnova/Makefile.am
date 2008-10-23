## Process this file with automake to produce Makefile.in

SUBDIRS = libnova

lib_LTLIBRARIES = libnova.la

libnova_la_SOURCES = \
	julian_day.c \
	dynamical_time.c \
	sidereal_time.c \
	transform.c \
	nutation.c \
	aberration.c   \
	apparent_position.c \
	solar.c \
	precession.c  \
	proper_motion.c \
	mercury.c \
	venus.c  \
	earth.c  \
	mars.c   \
	jupiter.c \
	saturn.c  \
	uranus.c  \
	neptune.c  \
	pluto.c \
	vsop87.c \
	lunar.c  \
	elliptic_motion.c \
	asteroid.c \
	comet.c \
	parabolic_motion.c \
	utility.c \
	refraction.c \
	rise_set.c \
	angular_separation.c \
	hyperbolic_motion.c \
	parallax.c \
	airmass.c

libnova_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-release $(LT_RELEASE) \
	-no-undefined \
	-export-dynamic
