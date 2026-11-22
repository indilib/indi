SET (test_base64_SRCS
	test_base64.cpp
)
ADD_EXECUTABLE(test_base64
    ${test_base64_SRCS}
)
TARGET_LINK_LIBRARIES(test_base64
    indiclient
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_base64 test_base64)

SET (test_tty_eof_SRCS
    test_tty_eof.cpp
)
ADD_EXECUTABLE(test_tty_eof
    ${test_tty_eof_SRCS}
)
TARGET_LINK_LIBRARIES(test_tty_eof
    indiclient
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_tty_eof test_tty_eof)

SET (test_property_class_SRCS
    test_property_class.cpp
)
ADD_EXECUTABLE(test_property_class
    ${test_property_class_SRCS}
)
TARGET_LINK_LIBRARIES(test_property_class
	indiclient
	${GTEST_BOTH_LIBRARIES}
	${GMOCK_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_property_class test_property_class)

SET (test_rotator_limits_SRCS
    test_rotator_limits.cpp
)
ADD_EXECUTABLE(test_rotator_limits
    ${test_rotator_limits_SRCS}
)
TARGET_LINK_LIBRARIES(test_rotator_limits
    ${GTEST_BOTH_LIBRARIES}
	${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_rotator_limits test_rotator_limits)

SET (test_libnova_nutation_SRCS
    test_libnova_nutation.cpp
)
ADD_EXECUTABLE(test_libnova_nutation ${test_libnova_nutation_SRCS})
TARGET_LINK_LIBRARIES(test_libnova_nutation
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_libnova_nutation test_libnova_nutation)

SET (test_libastro_SRCS
    test_libastro.cpp
)
ADD_EXECUTABLE(test_libastro ${test_libastro_SRCS})
TARGET_INCLUDE_DIRECTORIES(test_libastro PRIVATE ${CMAKE_SOURCE_DIR}/libs)
TARGET_LINK_LIBRARIES(test_libastro
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
TARGET_COMPILE_DEFINITIONS(test_libastro PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test/data"
)
ADD_TEST(test_libastro test_libastro)

# Built from source: indidriver provides its own main(), which would replace gtest's
SET (test_frame_tracker_SRCS
    test_frame_tracker.cpp
    ${CMAKE_SOURCE_DIR}/libs/indibase/stream/frametracker.cpp
)
ADD_EXECUTABLE(test_frame_tracker ${test_frame_tracker_SRCS})
TARGET_INCLUDE_DIRECTORIES(test_frame_tracker PRIVATE ${CMAKE_SOURCE_DIR}/libs/indibase)
TARGET_LINK_LIBRARIES(test_frame_tracker
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_frame_tracker test_frame_tracker)

# Regression test for oneBLOB enclen handling in the lilxml BLOB fast path
# (GHSA-j235-2w8x-gp3x, Finding 1). Links indiclient for the lilxml parser.
SET (test_lilxml_enclen_SRCS
    test_lilxml_enclen.cpp
)
ADD_EXECUTABLE(test_lilxml_enclen ${test_lilxml_enclen_SRCS})
TARGET_LINK_LIBRARIES(test_lilxml_enclen
    indiclient
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_lilxml_enclen test_lilxml_enclen)

# Regression test for oneBLOB enclen handling in driver dispatch()
# (GHSA-j235-2w8x-gp3x, Finding 2). dispatch()/IDDefBLOB/driverio are driver-side,
# so build them from source here; indidriver provides its own main(), so it is
# NOT linked - gtest's main() must win. indiclient supplies the indicore symbols.
SET (test_indidriver_blob_SRCS
    test_indidriver_blob.cpp
    ${CMAKE_SOURCE_DIR}/libs/indibase/indidriver.c
    ${CMAKE_SOURCE_DIR}/libs/indibase/indidriverio.c
)
ADD_EXECUTABLE(test_indidriver_blob ${test_indidriver_blob_SRCS})
TARGET_INCLUDE_DIRECTORIES(test_indidriver_blob PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/indibase
    ${CMAKE_SOURCE_DIR}/libs/indicore
)
TARGET_LINK_LIBRARIES(test_indidriver_blob
    indiclient
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_indidriver_blob test_indidriver_blob)
