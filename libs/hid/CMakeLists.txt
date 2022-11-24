cmake_minimum_required(VERSION 3.16)
project(hid C)

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    hidapi.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
)

# Sources
if(APPLE)
    list(APPEND ${PROJECT_NAME}_SOURCES hid_mac.c)
elseif(WIN32)
    list(APPEND ${PROJECT_NAME}_SOURCES hid_win.c)
else()
    list(APPEND ${PROJECT_NAME}_SOURCES hid_libusb.c)
endif()

# Setup Target
target_sources(${PROJECT_NAME}
    PUBLIC
    ${${PROJECT_NAME}_HEADERS}
    PRIVATE
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_PRIVATE_HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC .
)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)

# ########## HID Test ##############
if(APPLE)
    set(LIBS "-framework IOKit -framework CoreFoundation")
elseif(CYGWIN)
    set(LIBS ${ICONV_LIBRARIES})
endif()

add_executable(indi_hid_test hidtest.cpp)
target_link_libraries(indi_hid_test hid ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LIBS})
install(TARGETS indi_hid_test RUNTIME DESTINATION bin)
