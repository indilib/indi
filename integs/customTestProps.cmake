# This is used by ctest after test discovery occured
set_tests_properties(${TestIndiserverSingleDriver_TESTS} PROPERTIES
    TIMEOUT 5
)

set_tests_properties(${TestIndiClient_TESTS} PROPERTIES
    TIMEOUT 5
)
