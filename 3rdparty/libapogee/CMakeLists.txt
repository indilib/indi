cmake_minimum_required(VERSION 2.4.7)
PROJECT(libapogee C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

set(APOGEE_VERSION "3.0.2")
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
   ${CMAKE_CURRENT_SOURCE_DIR}/GenOneLinuxUSB.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/linuxHelpers.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/LinuxTimer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/LoggerSyslog.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/UdpSocketLinux.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaCcdAcqParams.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Alta.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaData.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaEthernetIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaF.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaModeFsm.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AltaUsbIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/apgHelper.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ApgLogger.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ApgTimer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ApnCamData.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ApogeeCam.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Ascent.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AscentBasedIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AscentBasedUsbIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AscentData.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamCfgMatrix.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CameraIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CameraInfo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CameraStatusRegs.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamGen2Base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamGen2CcdAcqParams.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamGen2ModeFsm.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamHelpers.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamRegMirror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CamUsbIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CcdAcqParams.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FilterWheelIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FindDeviceEthernet.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FindDeviceUsb.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Aspen.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AspenEthernetIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AspenIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AspenUsbIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/helpers.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/HiC.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IAltaSerialPortIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ICamIo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ILog.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ImgFix.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ITimer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/libCurlWrap.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/LoggerException.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ModeFsm.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/parseCfgTabDelim.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PlatformData.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IUsb.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/UdpSocketBase.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ApogeeFilterWheel.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PromFx2Io.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Quad.cpp
   )
  
add_library(apogee SHARED ${libapogee_SRCS})

set_target_properties(apogee PROPERTIES VERSION ${APOGEE_VERSION} SOVERSION ${APOGEE_SOVERSION})

target_link_libraries(apogee ${LIBUSB_1_LIBRARIES} ${CURL_LIBRARY} ${CURL_LIBRARY})

install(TARGETS apogee LIBRARY DESTINATION lib${LIB_POSTFIX} )

install( FILES ApogeeCam.h Alta.h AltaF.h Ascent.h CamGen2Base.h CameraInfo.h CameraStatusRegs.h Aspen.h FindDeviceEthernet.h FindDeviceUsb.h HiC.h ApogeeFilterWheel.h Quad.h DefDllExport.h versionNo.h doc.h DESTINATION ${INCLUDE_INSTALL_DIR}/libapogee COMPONENT Devel)

