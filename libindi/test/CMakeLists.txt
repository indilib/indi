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

# Build a library of INDI components we want to test.
ADD_LIBRARY (test_indidriver SHARED 
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
SET_TARGET_PROPERTIES (test_baader_dome PROPERTIES COMPILE_FLAGS "-fPIC")
TARGET_LINK_LIBRARIES (test_baader_dome
    indi 
    indiclient 
    test_indidriver
    gmock
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST (test_baader_dome test_baader_dome)



