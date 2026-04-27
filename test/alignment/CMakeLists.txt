INCLUDE_DIRECTORIES( ${INDI_INCLUDE_DIR} )

ADD_EXECUTABLE(test_alignment
    test_alignment.cpp
)

TARGET_LINK_LIBRARIES(test_alignment
    AlignmentDriver
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test-alignment test_alignment)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ERFA REQUIRED IMPORTED_TARGET erfa)

ADD_EXECUTABLE(test_alignment_plugins
    test_alignment_plugins.cpp
    ../../libs/alignment/BuiltInMathPlugin.cpp
    ../../libs/alignment/SVDMathPlugin.cpp
    ../../libs/alignment/NearestMathPlugin.cpp
    ../../libs/alignment/BasicMathPlugin.cpp
    ../../libs/alignment/SPKMathPlugin.cpp
    ../../libs/alignment/spk/vtel.c
    ../../libs/alignment/spk/pmfit.c
    ../../libs/alignment/spk/astr.c
    ../../drivers/telescope/scopesim_helper.cpp
)

TARGET_INCLUDE_DIRECTORIES(test_alignment_plugins PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/alignment
    ${CMAKE_SOURCE_DIR}/libs/alignment/spk
    ${CMAKE_SOURCE_DIR}/libs/indicore
    ${CMAKE_SOURCE_DIR}/drivers/telescope
)

TARGET_LINK_LIBRARIES(test_alignment_plugins
    AlignmentDriver
    indidriver
    PkgConfig::ERFA
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

TARGET_COMPILE_DEFINITIONS(test_alignment_plugins PRIVATE NO_PLUGIN_HOOKS)

ADD_TEST(test-alignment-plugins test_alignment_plugins)

