CMAKE_MINIMUM_REQUIRED (VERSION 3.0)

INCLUDE_DIRECTORIES ( ${GTEST_INCLUDE_DIRS} )

include(GoogleTest)

ENABLE_TESTING()


add_executable(fakedriver fakedriver.cpp utils.cpp)

add_executable(XmlAwaiterTest XmlAwaiterTest.cpp XmlAwaiter.cpp)
target_link_libraries(XmlAwaiterTest gtest_main)
gtest_discover_tests(XmlAwaiterTest PROPERTIES TIMEOUT 5)

add_executable(TestIndiserverSingleDriver TestIndiserverSingleDriver.cpp DriverMock.cpp ConnectionMock.cpp IndiClientMock.cpp IndiServerController.cpp utils.cpp XmlAwaiter.cpp SharedBuffer.cpp ServerMock.cpp)
target_link_libraries(TestIndiserverSingleDriver gtest_main)
gtest_discover_tests(TestIndiserverSingleDriver PROPERTIES TIMEOUT 5)

add_executable(TestIndiClient TestIndiClient.cpp DriverMock.cpp ConnectionMock.cpp IndiClientMock.cpp IndiServerController.cpp utils.cpp XmlAwaiter.cpp SharedBuffer.cpp ServerMock.cpp)
target_link_libraries(TestIndiClient gtest_main indiclient ${ZLIB_LIBRARY} ${NOVA_LIBRARIES})
IF (UNIX AND NOT APPLE)
    target_link_libraries(TestIndiClient -lpthread)
ENDIF ()
gtest_discover_tests(TestIndiClient PROPERTIES TIMEOUT 5)

# Inject properties for discovered tests
set_property(DIRECTORY APPEND PROPERTY
    TEST_INCLUDE_FILES ${CMAKE_CURRENT_LIST_DIR}/customTestProps.cmake
)
