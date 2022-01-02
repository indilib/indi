CMAKE_MINIMUM_REQUIRED (VERSION 3.0)

ENABLE_TESTING()

add_executable(servertester servertester.cpp FakeDriverCnx.cpp IndiServerCnx.cpp utils.cpp)

add_executable(fakedriver fakedriver.cpp utils.cpp)

add_test(servertester servertester)
set_tests_properties(servertester PROPERTIES WILL_FAIL FALSE)
set_tests_properties(servertester PROPERTIES TIMEOUT 10)

