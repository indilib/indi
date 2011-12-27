# set the include path found by configure

lib_LTLIBRARIES = libqsiapi.la 

libqsiapi_la_CPPFLAGS= $(all_includes)

libqsiapi_la_CXXFLAGS = -fPIC -g

include_HEADERS = qsiapi.h QSIError.h

noinst_HEADERS = CCDCamera.h CameraID.h ConvertUTF.h Filter.h FilterWheel.h HotPixelMap.h QSILog.h \
	QSI_Global.h QSI_Interface.h QSI_PacketWrapper.h QSI_Registry.h \
	QSI_States.h QSI_USBWrapper.h SimpleIni.h WinTypes.h ftd2xx.h wincompat.h

libqsiapi_la_SOURCES = CCDCamera.cpp CameraID.cpp ConvertUTF.c Filter.cpp \
	FilterWheel.cpp HotPixelMap.cpp HotPixelMap.h QSILog.cpp QSI_Interface.cpp \
	QSI_PacketWrapper.cpp QSI_USBWrapper.cpp qsiapi.cpp qsicopyright.txt

# version-info a:b:c yields a library name (a-c).c.b
# example (3:2:1) yields 2.1.2
libqsiapi_la_LDFLAGS = -version-info 6:3:0 $(all_libraries) -fPIC --shared

#uncomment below for ftd2xx library
#libqsiapi_la_LIBADD = -L/usr/local/lib -lftd2xx -lc 
#uncomment below for libftdi
libqsiapi_la_LIBADD = -L/usr/local/lib -lftdi -lc

libqsiapi_la_METASOURCES = AUTO

libqsiapi_la_DEPENDENCIES = 
