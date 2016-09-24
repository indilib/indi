cmake_minimum_required(VERSION 2.8)
PROJECT(libapogee C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(CONF_DIR "/etc/Apogee/camera" CACHE STRING "Configuration directory")

set(APOGEE_VERSION "3.0.3234")
set(APOGEE_SOVERSION "3")

find_package(USB-1 REQUIRED)
find_package(CURL REQUIRED)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS regex)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${Boost_INCLUDE_DIRS})
include_directories( ${CURL_INCLUDE_DIR})

set(libapogee_SRCS
   GenOneLinuxUSB.cpp
   linuxHelpers.cpp
   LinuxTimer.cpp
   LoggerSyslog.cpp
   UdpSocketLinux.cpp
   AltaCcdAcqParams.cpp
   Alta.cpp
   AspenData.cpp
   AltaData.cpp
   AltaEthernetIo.cpp
   AltaF.cpp
   AltaIo.cpp
   AltaModeFsm.cpp
   AltaUsbIo.cpp
   apgHelper.cpp
   ApgLogger.cpp
   ApgTimer.cpp
   ApnCamData.cpp
   ApogeeCam.cpp
   Ascent.cpp
   AscentBasedIo.cpp
   AscentBasedUsbIo.cpp
   AscentData.cpp
   CamCfgMatrix.cpp
   CameraIo.cpp
   CameraInfo.cpp
   CameraStatusRegs.cpp
   CamGen2Base.cpp
   CamGen2CcdAcqParams.cpp
   CamGen2ModeFsm.cpp
   CamHelpers.cpp
   CamRegMirror.cpp
   CamUsbIo.cpp
   CcdAcqParams.cpp
   FilterWheelIo.cpp
   FindDeviceEthernet.cpp
   FindDeviceUsb.cpp
   Aspen.cpp
   AspenEthernetIo.cpp
   AspenIo.cpp
   AspenUsbIo.cpp
   helpers.cpp
   HiC.cpp
   IAltaSerialPortIo.cpp
   ICamIo.cpp
   ILog.cpp
   ImgFix.cpp
   ITimer.cpp
   libCurlWrap.cpp
   LoggerException.cpp
   ModeFsm.cpp
   parseCfgTabDelim.cpp
   PlatformData.cpp
   IUsb.cpp
   UdpSocketBase.cpp
   ApogeeFilterWheel.cpp
   PromFx2Io.cpp
   Quad.cpp
   )

add_library(apogee SHARED ${libapogee_SRCS})

set_target_properties(apogee PROPERTIES VERSION ${APOGEE_VERSION} SOVERSION ${APOGEE_SOVERSION})

target_link_libraries(apogee ${LIBUSB_1_LIBRARIES} ${CURL_LIBRARY} ${Boost_LIBRARIES})

install(TARGETS apogee LIBRARY DESTINATION lib)

install( FILES ApogeeCam.h Alta.h AltaF.h Ascent.h CamGen2Base.h CameraInfo.h CameraStatusRegs.h Aspen.h FindDeviceEthernet.h FindDeviceUsb.h HiC.h ApogeeFilterWheel.h Quad.h DefDllExport.h versionNo.h doc.h ApgLogger.h LoggerSyslog.h DESTINATION include/libapogee COMPONENT Devel)

install(
   CODE "
   file(GLOB APOGEE_CONF ${CMAKE_CURRENT_SOURCE_DIR}/conf/*) \n
   file(INSTALL DESTINATION ${CONF_DIR} TYPE FILE FILES \${APOGEE_CONF})"
 )

install(FILES 99-apogee.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

