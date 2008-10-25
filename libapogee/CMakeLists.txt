cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CURL REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${CMAKE_SOURCE_DIR}/ApogeeNet)
include_directories( ${CMAKE_SOURCE_DIR}/ApogeeUsb)
include_directories( ${CMAKE_SOURCE_DIR}/FpgaRegs)

set(AltaCommon_SRCS
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF6303E.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI11000ML.cpp                                          
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF4202.cpp                                             
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020C.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD5520.cpp                                             
   ${CMAKE_SOURCE_DIR}/ApnCamData.cpp                                                     
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4710ALT.cpp                                          
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020C.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1301E.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4240B.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020CLB.cpp                                          
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT0402ME3.cpp                                       
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI11000CL.cpp                                          
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF09000.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2001CL.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF16801E.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2001ML.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020M.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF16803.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4720B.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1301EB.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD5710F.cpp                                            
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020CL.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1001EB.cpp                                           
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD7700.cpp                                             
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4240.cpp                                             
   ${CMAKE_SOURCE_DIR}/ApnCamData_S101401107.cpp                                          
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1001E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020MLB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4710B.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT16000.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF0401E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2001M.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1401E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI11000C.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF0261E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD5520B.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD5710B.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD5710.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI11000M.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1602EB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020MLB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF0261EB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020CL.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT4000.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF3200EB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF10011105.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD3011.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT2000.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1001ENS.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF6303EB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT0402ME2.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI4020ML.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020ML.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT340.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF0401EB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD7700B.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT0402ME4.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF3200E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_ASCENT0402ME.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_TH7899.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD3011B.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020M.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF1602E.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4720.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF09000X.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_CCD4710.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAI2020CLB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_KAF09000H.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamData_S101401109.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamTable.cpp
   )

add_definitions("-O2 -g -fpic -DWall -DLINUX -DALTA_STANDALONE -DHAVE_STRERROR")

########### libapogeeAltaU ###########
set(AltaU_SRCS
   ${CMAKE_SOURCE_DIR}/libapogee.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera_USB.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera_Linux.cpp
   ${CMAKE_SOURCE_DIR}/ApogeeUsb/ApogeeUsbLinux.cpp
   ${AltaCommon_SRCS}
   )
   
add_library(apogeeu SHARED ${AltaU_SRCS})

set_target_properties(apogeeu PROPERTIES VERSION 1.9 SOVERSION 1)

target_link_libraries(apogeeu ${LIBUSB_LIBRARIES})

install(TARGETS apogeeu LIBRARY DESTINATION lib${LIB_POSTFIX} )

 ########### libapogeeAltaE ###########
set(AltaE_SRCS
   ${CMAKE_SOURCE_DIR}/libapogee.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera_NET.cpp
   ${CMAKE_SOURCE_DIR}/ApnCamera_Linux.cpp
   ${CMAKE_SOURCE_DIR}/ApogeeNet/ApogeeNet.cpp
   ${CMAKE_SOURCE_DIR}/ApogeeNet/ApogeeNetLinux.c
   ${AltaCommon_SRCS}
   )
   
add_library(apogeee SHARED ${AltaE_SRCS})

set_target_properties(apogeee PROPERTIES VERSION 1.9 SOVERSION 1)

target_link_libraries(apogeee ${LIBUSB_LIBRARIES})

install(TARGETS apogeee LIBRARY DESTINATION lib${LIB_POSTFIX} )

install( FILES libapogee.h DESTINATION ${INCLUDE_INSTALL_DIR}/libapogee COMPONENT Devel)

