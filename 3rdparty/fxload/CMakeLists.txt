cmake_minimum_required(VERSION 3.0)
PROJECT(fxload CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(USB1 REQUIRED)
include_directories(${LIBUSB_1_INCLUDE_DIRS})

add_executable(fxload ezusb.c main.c)

target_link_libraries(fxload ${LIBUSB_1_LDFLAGS})

# CMAKE_INSTALL_SBINDIR installs it to /usr/sbin so have to set it to /sbin explicitly
install(TARGETS fxload RUNTIME DESTINATION /sbin )
