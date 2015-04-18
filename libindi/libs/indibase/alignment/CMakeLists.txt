set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

find_package(GSL REQUIRED)

if (GSL_FOUND)
	include_directories(${GSL_INCLUDE_DIRS})
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS GSL_FOUND)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${CMAKE_GSL_CXX_FLAGS})
endif (GSL_FOUND)

set(INDI_MATH_PLUGINS_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/indi/MathPlugins")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS INDI_MATH_PLUGINS_DIRECTORY="${INDI_MATH_PLUGINS_DIRECTORY}")

option(ALIGNMENT_CONVEX_HULL_DEBUGGING "Alignment subsystem - additional debugging output" OFF)

if(ALIGNMENT_CONVEX_HULL_DEBUGGING)
   set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS CONVEX_HULL_DEBUGGING)
endif(ALIGNMENT_CONVEX_HULL_DEBUGGING)

option(SKYWATCHER_API_USE_INITIAL_JULIAN_DATE "Skywatcher API - use initial Julian date only" OFF)

if(SKYWATCHER_API_USE_INITIAL_JULIAN_DATE)
   set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS USE_INITIAL_JULIAN_DATE)
endif(SKYWATCHER_API_USE_INITIAL_JULIAN_DATE)

##################################################
####### INDI AlignmentDriver shared library ######
##################################################
set (AlignmentDriver_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/AlignmentSubsystemForDrivers.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/AlignmentSubsystemForMathPlugins.h
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/BasicMathPlugin.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/BuiltInMathPlugin.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/ConvexHull.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/DriverCommon.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/InMemoryDatabase.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/MapPropertiesToInMemoryDatabase.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/MathPlugin.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/MathPluginManagement.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/TelescopeDirectionVectorSupportFunctions.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/Common.cpp
    )

add_library(AlignmentDriver SHARED ${AlignmentDriver_SRCS})
target_link_libraries(AlignmentDriver dl)
if (GSL_FOUND)
	target_link_libraries(AlignmentDriver ${GSL_LIBRARIES})
endif (GSL_FOUND)
set_target_properties(AlignmentDriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indiAlignmentDriver)
install(TARGETS AlignmentDriver LIBRARY DESTINATION ${LIB_DESTINATION})
install(FILES AlignmentSubsystemForMathPlugins.h AlignmentSubsystemForDrivers.h BasicMathPlugin.h BuiltInMathPlugin.h
              ClientAPIForAlignmentDatabase.h ClientAPIForMathPluginManagement.h Common.h ConvexHull.h DriverCommon.h InMemoryDatabase.h MathPlugin.h
              MathPluginManagement.h SVDMathPlugin.h TelescopeDirectionVectorSupportFunctions.h MapPropertiesToInMemoryDatabase.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/alignment COMPONENT Devel)

##################################################
####### INDI AlignmentClient static library ######
##################################################
set (AlignmentClient_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/AlignmentSubsystemForClients.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/ClientAPIForAlignmentDatabase.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/ClientAPIForMathPluginManagement.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/Common.h
    )

add_library(AlignmentClient ${AlignmentClient_SRCS})
set_target_properties(AlignmentClient PROPERTIES OUTPUT_NAME indiAlignmentClient)
install(TARGETS AlignmentClient ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
############ LoaderCLient test program ###########
##################################################
#set(LoaderClient_SRCS
#	${CMAKE_SOURCE_DIR}/libs/indibase/alignment/LoaderClient.cpp
#	${CMAKE_SOURCE_DIR}/libs/indibase/alignment/LoaderMain.cpp
#	)

#add_executable(LoaderClient ${LoaderClient_SRCS})

#target_link_libraries(LoaderClient indiclient AlignmentClient)

#install(TARGETS LoaderClient RUNTIME DESTINATION bin)

##################################################
######### MathPluginManager test program #########
##################################################
#set(MathPluginManagerClient_SRCS
#	${CMAKE_SOURCE_DIR}/libs/indibase/alignment/MathPluginManagerClient.cpp
#	${CMAKE_SOURCE_DIR}/libs/indibase/alignment/MathPluginManagerMain.cpp
#	)

#add_executable(MathPluginManagerClient ${MathPluginManagerClient_SRCS})

#target_link_libraries(MathPluginManagerClient indiclient AlignmentClient)

#install(TARGETS MathPluginManagerClient RUNTIME DESTINATION bin)

##################################################
########### Dummy math plugin example ############
##################################################
#set(DummyMathPlugin_SRCS
#	${CMAKE_SOURCE_DIR}/libs/indibase/alignment/DummyMathPlugin.cpp
#	)

#add_library(indi_Dummy_MathPlugin SHARED ${DummyMathPlugin_SRCS})

#target_link_libraries(indi_Dummy_MathPlugin indidriver)

#install(TARGETS indi_Dummy_MathPlugin LIBRARY DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY})

##################################################
################ SVD math plugin #################
##################################################
set(SVDMathPlugin_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/alignment/SVDMathPlugin.cpp
	)

add_library(indi_SVD_MathPlugin SHARED ${SVDMathPlugin_SRCS})

set_property(TARGET indi_SVD_MathPlugin APPEND PROPERTY COMPILE_DEFINITIONS SVD_TRANSFORM_MATRIX)

target_link_libraries(indi_SVD_MathPlugin indidriver)

install(TARGETS indi_SVD_MathPlugin LIBRARY DESTINATION ${INDI_MATH_PLUGINS_DIRECTORY})

##################################################
############ Skywatcher API Mount ################
##################################################
set(skywatcherAPIMount_SRCS
   ${CMAKE_SOURCE_DIR}/drivers/telescope/skywatcherAPIMount.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/skywatcherAPI.cpp )

add_executable(indi_skywatcherAPIMount ${skywatcherAPIMount_SRCS})
target_link_libraries(indi_skywatcherAPIMount indidriver AlignmentDriver)

install(TARGETS indi_skywatcherAPIMount RUNTIME DESTINATION bin )


