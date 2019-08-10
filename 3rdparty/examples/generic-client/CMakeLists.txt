cmake_minimum_required(VERSION 3.0)
PROJECT(generic_client CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI COMPONENTS client REQUIRED)
find_package(Threads REQUIRED)
find_package(Nova REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

########### Generic Client ###########
set(indi_generic_client_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/generic_client.cpp
   )

add_executable(indi_generic_client ${indi_generic_client_SRCS})
target_link_libraries(indi_generic_client ${INDI_LIBRARIES} ${NOVA_LIBRARIES} z)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(indi_generic_client "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(indi_generic_client "${CMAKE_THREAD_LIBS_INIT}")
endif()

install(TARGETS indi_generic_client RUNTIME DESTINATION bin)
