

SET (test_base64_SRCS
	test_base64.cpp
)

SET(test_connectionplugins_SRCS
        test_connectiontcp.cpp
        )

ADD_EXECUTABLE(test_base64
	${test_base64_SRCS}
        )
ADD_EXECUTABLE(test_connectionplugins
        ${test_connectionplugins_SRCS}
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/indibase/connectionplugins/connectiontcp"
)
TARGET_LINK_LIBRARIES(test_base64
	indiclient
	${GTEST_BOTH_LIBRARIES}
        #	${GMOCK_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        )

TARGET_LINK_LIBRARIES(test_connectionplugins
        indidriver
        ${GTEST_BOTH_LIBRARIES}
        #	${GMOCK_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
)


ADD_TEST(test_base64 test_base64)
ADD_TEST(test_connectionplugins test_connectionplugins)


