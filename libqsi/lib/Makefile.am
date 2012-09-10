# set the include path found by configure

lib_LTLIBRARIES = libqsiapi.la 

# ./configure selects the libftdi-0.1 stack
# ./configure --enable-ftd2xx option selects the libftd2xx stack
# ENABLE_FTD2XX is set in configure.ac

if ENABLE_LIBFTD2XX
FTD2XXSTACK = -Wc,-DUSELIBFTD2XX
else 
FTD2XXSTACK =
endif

if ENABLE_LIBFTDI
FTDISTACK = -Wc,-DUSELIBFTDIZERO
else
FTDISTACK =
endif

libqsiapi_la_CPPFLAGS= $(all_includes) $(FTD2XX_CFLAGS) $(FTDI_CFLAGS) $(FTDISTACK) $(FTD2XXSTACK)

libqsiapi_la_CXXFLAGS = -fPIC -g -fpermissive

include_HEADERS = qsiapi.h QSIError.h

noinst_HEADERS = CCDCamera.h CameraID.h ConvertUTF.h Filter.h FilterWheel.h HotPixelMap.h QSILog.h \
	QSI_Global.h QSI_Interface.h QSI_PacketWrapper.h QSI_Registry.h \
	QSI_States.h QSI_USBWrapper.h SimpleIni.h WinTypes.h ftd2xx.h wincompat.h QSICriticalSection.h QSIFeatures.h HotPixelMap.h

libqsiapi_la_SOURCES = CCDCamera.cpp CameraID.cpp ConvertUTF.c Filter.cpp \
	FilterWheel.cpp HotPixelMap.cpp QSILog.cpp QSI_Interface.cpp \
	QSI_PacketWrapper.cpp QSI_USBWrapper.cpp qsiapi.cpp qsicopyright.txt QSIFeatures.cpp

# version-info a:b:c yields a library name (a-c).c.b
# example (3:2:1) yields 2.1.2
#libqsiapi_la_LDFLAGS = -version-info 0:0:0 $(all_libraries) -fPIC --shared

libqsiapi_la_LIBADD = -L/usr/local/lib $(FTD2XX_LIBS) $(FTDI_LIBS) -lc

libqsiapi_la_METASOURCES = AUTO

libqsiapi_la_DEPENDENCIES = 
