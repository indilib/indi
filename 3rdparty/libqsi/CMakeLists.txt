PROJECT(libqsi C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

FIND_PACKAGE(FTDI REQUIRED)

SET(PACKAGE_VERSION "6.4.1")
SET(USELIBFTDIZERO "1")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

ADD_DEFINITIONS(-Wall -O2 -fPIC -g -fpermissive)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

set(qsi_LIB_SRCS
   CameraID.cpp CCDCamera.cpp Filter.cpp FilterWheel.cpp HotPixelMap.cpp qsiapi.cpp QSIFeatures.cpp QSI_Interface.cpp QSILog.cpp QSI_PacketWrapper.cpp QSI_USBWrapper.cpp
   ConvertUTF.c
)

#build a shared library
ADD_LIBRARY(qsiapi SHARED ${qsi_LIB_SRCS})

set_target_properties(qsiapi PROPERTIES VERSION 6.4.1 SOVERSION 6)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(qsiapi ${FTDI_LIBRARIES})

#add an install target here
INSTALL(FILES qsiapi.h QSIError.h DESTINATION include)

INSTALL(TARGETS qsiapi LIBRARY DESTINATION lib)

# build API test
set(qsiapitest_SRCS
   ${qsi_LIB_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/demo_src/qsiapitest.cpp )

add_executable(qsiapitest ${qsiapitest_SRCS})

TARGET_LINK_LIBRARIES(qsiapitest ${FTDI_LIBRARIES})

install(TARGETS qsiapitest RUNTIME DESTINATION bin )


# build demo
set(qsidemo_SRCS
   ${qsi_LIB_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/demo_src/qsidemo.cpp)

add_executable(qsidemo ${qsidemo_SRCS})

TARGET_LINK_LIBRARIES(qsidemo ${FTDI_LIBRARIES})

install(TARGETS qsidemo RUNTIME DESTINATION bin )
