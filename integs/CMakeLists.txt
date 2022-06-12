CMAKE_MINIMUM_REQUIRED (VERSION 3.0)

FIND_PACKAGE (GTest REQUIRED)

INCLUDE_DIRECTORIES ( ${GTEST_INCLUDE_DIRS} )

include(GoogleTest)

ENABLE_TESTING()


MESSAGE (STATUS "INTEG:GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS}")
MESSAGE (STATUS "INTEG:GTEST_LIBRARIES ${GTEST_LIBRARIES}")


# Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
STRING(REPLACE "-pie" "" CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})

add_executable(fakedriver fakedriver.cpp utils.cpp)

add_executable(XmlAwaiterTest XmlAwaiterTest.cpp XmlAwaiter.cpp)
target_link_libraries(XmlAwaiterTest ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
gtest_discover_tests(XmlAwaiterTest PROPERTIES TIMEOUT 5)

add_executable(TestIndiserverSingleDriver TestIndiserverSingleDriver.cpp DriverMock.cpp ConnectionMock.cpp IndiClientMock.cpp IndiServerController.cpp utils.cpp XmlAwaiter.cpp SharedBuffer.cpp ServerMock.cpp)
target_link_libraries(TestIndiserverSingleDriver ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
gtest_discover_tests(TestIndiserverSingleDriver PROPERTIES TIMEOUT 5)

add_executable(TestIndiClient TestIndiClient.cpp DriverMock.cpp ConnectionMock.cpp IndiClientMock.cpp IndiServerController.cpp utils.cpp XmlAwaiter.cpp SharedBuffer.cpp ServerMock.cpp)
target_link_libraries(TestIndiClient indiclient ${GTEST_BOTH_LIBRARIES} ${ZLIB_LIBRARY} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
gtest_discover_tests(TestIndiClient PROPERTIES TIMEOUT 5)

# Inject properties for discovered tests
set_property(DIRECTORY APPEND PROPERTY
    TEST_INCLUDE_FILES ${CMAKE_CURRENT_LIST_DIR}/customTestProps.cmake
)
