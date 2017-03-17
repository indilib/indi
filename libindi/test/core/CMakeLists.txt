

SET (test_base64_SRCS
	test_base64.cpp
)


ADD_EXECUTABLE(test_base64
	${test_base64_SRCS}
)
TARGET_LINK_LIBRARIES(test_base64
	indiclient
	indidriver
	${GTEST_BOTH_LIBRARIES}
	${GMOCK_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)


ADD_TEST(test_base64 test_base64)


