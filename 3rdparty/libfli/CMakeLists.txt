#this is just a basic CMakeLists.txt, for more information see the cmake manpage
cmake_minimum_required(VERSION 2.4.7)

ADD_DEFINITIONS(-Wall -O2)

set(fli_LIB_SRCS
   libfli.c libfli-camera.c libfli-camera-parport.c libfli-camera-usb.c
   libfli-filter-focuser.c libfli-mem.c libfli-serial.c libfli-sys.c
   libfli-usb.c libfli-debug.c libfli-parport.c libfli-usb-sys.c
   flifilter.c libfli-raw.c
)

#build a shared library
ADD_LIBRARY(fli SHARED ${fli_LIB_SRCS})

set_target_properties(fli PROPERTIES VERSION 1.8 SOVERSION 1)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(fli)

#add an install target here
INSTALL(FILES libfli.h DESTINATION include)

INSTALL(TARGETS fli LIBRARY DESTINATION lib${LIB_POSTFIX})
