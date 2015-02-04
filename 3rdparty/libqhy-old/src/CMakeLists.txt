set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules/")

set(FIRMWARE_INSTALL_DIR "/lib/firmware")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(UDEVRULES_INSTALL_DIR "/etc/udev/rules.d")
endif() 

set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)

find_package(USB-1 REQUIRED)
find_package(OpenCV REQUIRED)

include_directories($(USB_1_INCLUDE_DIR))
include_directories($(OpenCV_INCLUDE_DIR))

set(QHYCCD_SRC
   qhyccd.cpp
   qhycam.cpp
   qhybase.cpp 
   qhy5lii_c.cpp
   ic8300.cpp
   qhy21.cpp
   qhy22.cpp
   qhy8l.cpp
   qhy10.cpp
   qhy12.cpp
   qhy9s.cpp
   qhy8pro.cpp
   qhy11.cpp
   qhy23.cpp
   img2p.cpp
   qhy6.cpp
   qhy5ii.cpp
   qhy5lii_m.cpp
   qhy8.cpp
   qhyxxx.cpp
   simu.cpp
   )

set(QHYCCD_INCLUDE
    qhyccd.h
    qhyccdcamdef.h
    qhyccderr.h
    qhyccdstruct.h
   )


add_library(qhyccd SHARED ${QHYCCD_SRC})

set_target_properties(qhyccd PROPERTIES VERSION ${VERSION_MAJOR} SOVERSION ${VERSION_MAJOR}.${VERSION_MINOR})

target_link_libraries(qhyccd ${OpenCV_LIBRARIES} ${LIBUSB_1_LIBRARIES})

install(TARGETS qhyccd LIBRARY DESTINATION lib)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  configure_file(${PROJECT_SOURCE_DIR}/85-qhyccdtemplete.rules ${PROJECT_SOURCE_DIR}/85-qhyccd.rules @ONLY)
  install (FILES ${PROJECT_SOURCE_DIR}/85-qhyccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif ()

file(GLOB FIRMWARE "${PROJECT_SOURCE_DIR}/firmware/*.HEX")
install (FILES ${FIRMWARE} DESTINATION ${FIRMWARE_INSTALL_DIR})
install(FILES ${QHYCCD_INCLUDE} DESTINATION include/libqhyccd)

set(CPACK_PACKAGE_NAME "qhyccdsdk")
set(CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
set(CPACK_PACKAGE_CONTACT "Zhirong Li <lzr@qhyccd.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "QHYCCD SDK for qhyccd's CCD or CMOS.")


