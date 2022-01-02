CMAKE_MINIMUM_REQUIRED (VERSION 3.0)

INCLUDE_DIRECTORIES ( ${GTEST_INCLUDE_DIRS} )

include(GoogleTest)

ENABLE_TESTING()


add_executable(fakedriver fakedriver.cpp utils.cpp)

add_executable(TestIndiserverSingleDriver TestIndiserverSingleDriver.cpp FakeDriverCnx.cpp IndiServerCnx.cpp utils.cpp)
target_link_libraries(TestIndiserverSingleDriver gtest_main)

gtest_discover_tests(TestIndiserverSingleDriver PROPERTIES TIMEOUT 5)

# Inject properties for discovered tests
set_property(DIRECTORY APPEND PROPERTY
    TEST_INCLUDE_FILES ${CMAKE_CURRENT_LIST_DIR}/customTestProps.cmake
)
