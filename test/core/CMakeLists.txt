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
