CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

#ADD_SUBDIRECTORY(googletest)

ENABLE_TESTING ()

INCLUDE_DIRECTORIES ( . ${GTEST_INCLUDE_DIRS} )

SET (test_baader_dome_SRC
    "${PROJECT_SOURCE_DIR}/drivers/dome/baader_dome.cpp"
    "${PROJECT_SOURCE_DIR}/test/mocks/mock_indi_tty.cpp"
    "${PROJECT_SOURCE_DIR}/test/drivers/dome/baader/getDefaultName.cpp"
    "${PROJECT_SOURCE_DIR}/test/drivers/dome/baader/Connect.cpp"
)

#SET_PROPERTY (TARGET test_baader_dome APPEND_STRING PROPERTY COMPILE_FLAGS "-fPIC -Wall -Wextra -Wno-unused-variable -g -O3")   

ADD_LIBRARY (test_indidriver SHARED 
    ##${indimain_SRCS} 
    ${PROJECT_SOURCE_DIR}/indidriver.c
    ${CMAKE_SOURCE_DIR}/indidrivermain_gbls.c
    ${PROJECT_SOURCE_DIR}/eventloop.c
    ${liblilxml_SRCS}
    ${indidriver_SRCS} 
    ${libwebcam_SRCS} 
    ${hidapi_SRCS}    
)
SET_TARGET_PROPERTIES (test_indidriver PROPERTIES COMPILE_FLAGS "-fPIC")
TARGET_LINK_LIBRARIES (test_indidriver
    ${LIBUSB_1_LIBRARIES} 
    ${NOVA_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT} 
    ${CFITSIO_LIBRARIES} 
    ${M_LIB} ${ZLIB_LIBRARY} 
    ${JPEG_LIBRARY}
)

ADD_EXECUTABLE (test_baader_dome ${test_baader_dome_SRC})
TARGET_LINK_LIBRARIES (test_baader_dome
    indi 
    indiclient 
    #indidriver
    test_indidriver
    gmock
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    #${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY}
)
ADD_TEST (test_baader_dome test_baader_dome)



