set(INDI_MATH_PLUGINS_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/indi/MathPlugins")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS INDI_MATH_PLUGINS_DIRECTORY="${INDI_MATH_PLUGINS_DIRECTORY}")

option(ALIGNMENT_CONVEX_HULL_DEBUGGING "Alignment subsystem - additional debugging output" OFF)

if(ALIGNMENT_CONVEX_HULL_DEBUGGING)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS CONVEX_HULL_DEBUGGING)
endif(ALIGNMENT_CONVEX_HULL_DEBUGGING)

# #################################################
# ###### INDI AlignmentDriver shared library ######
# #################################################
SET(AlignmentDriver_SRC
    AlignmentSubsystemForDrivers.cpp
    AlignmentSubsystemForMathPlugins.h
    BasicMathPlugin.cpp
    BuiltInMathPlugin.cpp
    ConvexHull.cpp
    DriverCommon.cpp
    InMemoryDatabase.cpp
    MapPropertiesToInMemoryDatabase.cpp
    MathPlugin.cpp
    MathPluginManagement.cpp
    TelescopeDirectionVectorSupportFunctions.cpp
    Common.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(AlignmentDriver AlignmentDriver_SRC 15 cpp)
ENDIF()

add_library(AlignmentDriver SHARED ${AlignmentDriver_SRC})
set_target_properties(AlignmentDriver PROPERTIES COMPILE_FLAGS "-fPIC")

IF(APPLE)
    target_link_libraries(AlignmentDriver dl -L/usr/local/lib ${GSL_LIBRARIES})
ELSE()
    # Force linking all referenced libraries because of libgsl is not linked against cblas library on Linux
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed")
    target_link_libraries(AlignmentDriver dl ${GSL_LIBRARIES}
        $<$<PLATFORM_ID:CYGWIN>:indidriver>
    )
ENDIF()

target_link_libraries(AlignmentDriver indidevice indiclient)

if(INDI_BUILD_QT5)
    target_link_libraries(AlignmentDriver Qt5::Network)
endif(INDI_BUILD_QT5)

set_target_properties(AlignmentDriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indiAlignmentDriver)
install(TARGETS AlignmentDriver
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
    AlignmentSubsystemForMathPlugins.h
    AlignmentSubsystemForDrivers.h
    BasicMathPlugin.h
    BuiltInMathPlugin.h
    NearestMathPlugin.h
    ClientAPIForAlignmentDatabase.h
    ClientAPIForMathPluginManagement.h
    Common.h
    ConvexHull.h
    DriverCommon.h
    InMemoryDatabase.h
    MathPlugin.h
    MathPluginManagement.h
    SVDMathPlugin.h
    TelescopeDirectionVectorSupportFunctions.h
    MapPropertiesToInMemoryDatabase.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/alignment COMPONENT Devel)

# #################################################
# ###### INDI AlignmentClient static library ######
# #################################################
set(AlignmentClient_SRCS
    AlignmentSubsystemForClients.cpp
    ClientAPIForAlignmentDatabase.cpp
    ClientAPIForMathPluginManagement.cpp
    Common.h
)

add_library(AlignmentClient STATIC ${AlignmentClient_SRCS})

if(INDI_BUILD_QT5)
    target_link_libraries(AlignmentClient Qt5::Network)
endif(INDI_BUILD_QT5)

if(NOT WIN32)
    SET_TARGET_PROPERTIES(AlignmentClient PROPERTIES COMPILE_FLAGS "-fPIC")
endif(NOT WIN32)

target_link_libraries(AlignmentClient indidevice indiclient)

set_target_properties(AlignmentClient PROPERTIES OUTPUT_NAME indiAlignmentClient)
install(TARGETS AlignmentClient ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# #################################################
# ########### LoaderCLient test program ###########
# #################################################
# set(LoaderClient_SRCS
# LoaderClient.cpp
# LoaderMain.cpp
# )

# add_executable(LoaderClient ${LoaderClient_SRCS})

# target_link_libraries(LoaderClient indiclient AlignmentClient)

# install(TARGETS LoaderClient RUNTIME DESTINATION bin)

# #################################################
# ######## MathPluginManager test program #########
# #################################################
# set(MathPluginManagerClient_SRCS
# MathPluginManagerClient.cpp
# MathPluginManagerMain.cpp
# )

# add_executable(MathPluginManagerClient ${MathPluginManagerClient_SRCS})

# target_link_libraries(MathPluginManagerClient indiclient AlignmentClient)

# install(TARGETS MathPluginManagerClient RUNTIME DESTINATION bin)

# #################################################
# ########## Dummy math plugin example ############
# #################################################
# set(DummyMathPlugin_SRCS
# DummyMathPlugin.cpp
# )

# add_library(indi_Dummy_MathPlugin SHARED ${DummyMathPlugin_SRCS})

# target_link_libraries(indi_Dummy_MathPlugin indidriver)

# install(TARGETS indi_Dummy_MathPlugin LIBRARY DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY})

# #################################################
# ############### Nearest math plugin #############
# #################################################
set(NearestMathPlugin_SRCS
    NearestMathPlugin.cpp
)

add_library(indi_Nearest_MathPlugin SHARED ${NearestMathPlugin_SRCS})
target_include_directories(indi_Nearest_MathPlugin
    PRIVATE
        ${libindi_SOURCE_DIR}/libs/indicore
)
target_link_libraries(indi_Nearest_MathPlugin
    indidriver
    $<$<PLATFORM_ID:CYGWIN>:AlignmentDriver>
)

install(TARGETS indi_Nearest_MathPlugin
    ARCHIVE DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY}
    LIBRARY DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY}
    RUNTIME DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY})

# #################################################
# ############### SVD math plugin #################
# #################################################
set(SVDMathPlugin_SRCS
    SVDMathPlugin.cpp
)

add_library(indi_SVD_MathPlugin SHARED ${SVDMathPlugin_SRCS})
set_property(TARGET indi_SVD_MathPlugin APPEND PROPERTY COMPILE_DEFINITIONS SVD_TRANSFORM_MATRIX)
target_link_libraries(indi_SVD_MathPlugin
    indidriver
    $<$<PLATFORM_ID:CYGWIN>:AlignmentDriver>
)

install(TARGETS indi_SVD_MathPlugin
    ARCHIVE DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY}
    LIBRARY DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY}
    RUNTIME DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY})
