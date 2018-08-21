cmake_minimum_required(VERSION 3.0)
PROJECT(indi_nightscape CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(Threads REQUIRED)
FIND_PACKAGE(D2XX)
FIND_PACKAGE(USB1 REQUIRED)
FIND_PACKAGE(FTDI1 REQUIRED)

IF (D2XX_FOUND)
set(HAVE_D2XX 1)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_D2XX")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_D2XX")

ENDIF()

IF (APPLE)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DARWIN_C_SOURCE")
ENDIF()


set(INDI_NIGHTSCAPE_VERSION_MAJOR 1)
set(INDI_NIGHTSCAPE_VERSION_MINOR 0)

#set (HAVE_SERIAL 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_nightscape.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_nightscape.xml )


include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${USB1_INCLUDE_DIRS})
include_directories( ${FTDI1_INCLUDE_DIRS})

include(CMakeCommon)


SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-error")

SET(indinightscape_CORE
        ${CMAKE_CURRENT_SOURCE_DIR}/nschannel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-u.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nsmsg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nsdownload.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nsstatus.cpp)

IF(HAVE_D2XX) 
	SET(indinightscape_CORE
	     ${indinightscape_CORE}
       ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-ftd.cpp)
ENDIF()

IF(HAVE_SERIAL)
	SET(indinightscape_CORE
	     ${indinightscape_CORE}
        ${CMAKE_CURRENT_SOURCE_DIR}/nschannel-ser.cpp)
  
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SERIAL")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_SERIAL")

ENDIF()

SET(indinightscape_SRCS
        ${indinightscape_CORE}
        ${CMAKE_CURRENT_SOURCE_DIR}/nightscape.cpp)

SET(nstest_SRCS
        ${indinightscape_CORE}
        ${CMAKE_CURRENT_SOURCE_DIR}/nstest-main.cpp)


add_executable(indi_nightscape_ccd ${indinightscape_SRCS})

add_executable(nstest ${nstest_SRCS})

IF(HAVE_D2XX)
	target_link_libraries(indi_nightscape_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${D2XX_LIBRARIES} ${FTDI1_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

	target_link_libraries(nstest ${D2XX_LIBRARIES} ${FTDI1_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ELSE()
	target_link_libraries(indi_nightscape_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${FTDI1_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

	target_link_libraries(nstest ${FTDI1_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

install(TARGETS indi_nightscape_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_nightscape.xml DESTINATION ${INDI_DATA_DIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-nightscape.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
