cmake_minimum_required(VERSION 2.4.7)
PROJECT(QHYXXXDEMO)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../../cmake_modules/")

find_package(USB-1 REQUIRED)
find_package(OpenCV REQUIRED)

include_directories($(USB_1_INCLUDE_DIR))
include_directories($(OpenCV_INCLUDE_DIR))

aux_source_directory(. SRC_LIST)

foreach(F ${SRC_LIST})
    string(LENGTH ${F} NAMELENGTH)
    string(SUBSTRING ${F} 0 ${NAMELENGTH} FILENAME)
    string(REPLACE .cpp test FILENAME1 ${FILENAME})
    add_executable(${FILENAME1} ${F})
    target_link_libraries(${FILENAME1} -lqhyccd ${OpenCV_LIBRARIES} ${LIBUSB_1_LIBRARIES})
endforeach(F)
