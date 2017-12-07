
# Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")

ADD_EXECUTABLE(test_celestrondriver test_celestrondriver.cpp)

TARGET_LINK_LIBRARIES(test_celestrondriver
    indiclient ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test-celestrondriver test_celestrondriver)
