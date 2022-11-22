cmake_minimum_required(VERSION 3.16)
project(indiserver CXX)

if(WIN32 OR ANDROID)
    message(WARNING "INDI Server is only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
else()
    find_package(Threads REQUIRED)
    find_package(Libev REQUIRED)

    add_executable(${PROJECT_NAME} indiserver.cpp)

    target_link_libraries(indiserver indicore ${CMAKE_THREAD_LIBS_INIT} ${LIBEV_LIBRARIES})
    target_include_directories(indiserver SYSTEM PRIVATE ${LIBEV_INCLUDE_DIRS})

    install(TARGETS indiserver RUNTIME DESTINATION bin)
endif(WIN32 OR ANDROID)
