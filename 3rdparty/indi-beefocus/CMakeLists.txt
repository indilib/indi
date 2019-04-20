cmake_minimum_required(VERSION 2.8)
PROJECT(indi_beefocus CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)

set(BEEFOCUS_VERSION_MAJOR 1)
set(BEEFOCUS_VERSION_MINOR 0)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_beefocus.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_beefocus.xml )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/driver)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/firmware)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/glue)
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

################# Beefocus ############################

set(indi_beefocus_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/driver/beefocus.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/driver/beeconnect.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/driver/beesimfirmware.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/firmware/command_parser.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/firmware/focuser_state.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/firmware/hardware_interface.cpp
)

add_executable(indi_beefocus ${indi_beefocus_SRCS})

target_link_libraries(indi_beefocus ${INDI_LIBRARIES} )

install(TARGETS indi_beefocus RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_beefocus.xml DESTINATION ${INDI_DATA_DIR})

################# Unit Testing ########################

IF (INDI_BUILD_UNITTESTS)
  # Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
  # set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")
 
  enable_testing()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  SET(UNIT_TESTS 
    test_driver 
  )

  foreach( TEST ${UNIT_TESTS} )
    SET( TEST_SOURCES ${indi_beefocus_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/test_helpers.cpp )
    SET( TEST_MAIN_CPP ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/${TEST}.cpp)
    SET_SOURCE_FILES_PROPERTIES(${TEST_MAIN_CPP} PROPERTIES LANGUAGE CXX)

    ADD_EXECUTABLE(${TEST} ${TEST_MAIN_CPP} ${TEST_SOURCES})

    TARGET_LINK_LIBRARIES( ${TEST}
      ${INDI_LIBRARIES}
      ${GTEST_BOTH_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      #${ZLIB_LIBRARY}
    )
    add_test( run-tests ${TEST} )

    ADD_CUSTOM_COMMAND(
      TARGET ${TEST}
      COMMENT "Running ${TEST}"
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND ${TEST} )

  endforeach(TEST)

ENDIF (INDI_BUILD_UNITTESTS)


