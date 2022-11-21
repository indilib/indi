# ###################################################################################################
#
# Component   : LX200 Library
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
# ################################################################################################
SET(lx200library_SRCS
    lx200driver.cpp
    lx200telescope.cpp
)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(lx200library lx200library_SRCS 10 cpp)
ENDIF()

add_library(indilx200 SHARED ${lx200library_SRCS})

if(NOT CYGWIN AND NOT WIN32)
    set_target_properties(indilx200 PROPERTIES COMPILE_FLAGS "-fPIC")
endif(NOT CYGWIN AND NOT WIN32)

target_compile_definitions(indilx200 PRIVATE "-DHAVE_LIBNOVA")
set_target_properties(indilx200 PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indilx200)

install(TARGETS indilx200 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    lx200telescope.h
    lx200driver.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/mounts
    COMPONENT Devel)

# ################################################################################

# ########## LX200 Basic #############
SET(lx200basic_SRC
    lx200driver.cpp
    lx200basic.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(lx200basic lx200basic_SRC 10 cpp)
ENDIF()

add_executable(indi_lx200basic ${lx200basic_SRC})

target_link_libraries(indi_lx200basic indidriver)

install(TARGETS indi_lx200basic RUNTIME DESTINATION bin)

# ################################################################################

# ########## LX200 TeenAstro #############
SET(lx200_TeenAstro_SRC
    lx200driver.cpp
    lx200_TeenAstro.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(lx200_TeenAstro lx200_TeenAstro_SRC 10 cpp)
ENDIF()

add_executable(indi_lx200_TeenAstro ${lx200_TeenAstro_SRC})

target_link_libraries(indi_lx200_TeenAstro indidriver)

install(TARGETS indi_lx200_TeenAstro RUNTIME DESTINATION bin)

# ################################################################################

# ########## LX200 Generic ###########
SET(lx200generic_SRCS
    lx200driver.cpp
    lx200autostar.cpp
    lx200_16.cpp
    lx200gps.cpp
    lx200generic.cpp
    lx200telescope.cpp
    lx200classic.cpp
    lx200gemini.cpp
    lx200zeq25.cpp
    lx200am5.cpp
    lx200gotonova.cpp
    lx200pulsar2.cpp
    lx200apdriver.cpp
    lx200ap.cpp
    lx200ap_v2.cpp
    lx200ap_gtocp2.cpp
    lx200fs2.cpp
    lx200ss2000pc.cpp
    lx200_OnStep.cpp
    lx200_OpenAstroTech.cpp
    lx200_10micron.cpp
    ioptronHC8406.cpp
    eq500x.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(lx200generic lx200generic_SRCS 10 cpp)
ENDIF()

add_executable(indi_lx200generic ${lx200generic_SRCS})

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_compile_definitions(indi_lx200generic PRIVATE -D_XOPEN_SOURCE=700)
ELSE()
    target_compile_definitions(indi_lx200generic PRIVATE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L)
ENDIF()

target_link_libraries(indi_lx200generic indidriver)

install(TARGETS indi_lx200generic RUNTIME DESTINATION bin)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake
    "exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200autostar)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gps)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_v2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_gtocp2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gemini)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200zeq25)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200am5)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gotonova)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200pulsar2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ss2000pc)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OnStep)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OpenAstroTech)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_10micron)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_ioptronHC8406)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_eq500x_telescope)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)

# ################################################################################

# ########## Celestron GPS ############
SET(celestrongps_SRC
    celestrondriver.cpp
    celestrongps.cpp)

add_executable(indi_celestron_gps ${celestrongps_SRC})

target_link_libraries(indi_celestron_gps indidriver)

install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin)

# ########## Rainbow Mount driver ##########
SET(rainbow_SRC
    rainbow.cpp
    lx200driver.cpp)

add_executable(indi_rainbow_telescope ${rainbow_SRC})

target_link_libraries(indi_rainbow_telescope indidriver)

install(TARGETS indi_rainbow_telescope RUNTIME DESTINATION bin)

# ########## HOBYM / TitanTCS for CRUX Mount ##########
SET(crux_mount_SRC
    crux_mount.cpp
)

add_executable(indi_crux_mount ${crux_mount_SRC})

target_link_libraries(indi_crux_mount indidriver)

install(TARGETS indi_crux_mount RUNTIME DESTINATION bin)

# ########## Takahashi Temma ##########
SET(temma_SRC
    temmadriver.cpp)

add_executable(indi_temma_telescope ${temma_SRC})

target_link_libraries(indi_temma_telescope indidriver AlignmentDriver)

install(TARGETS indi_temma_telescope RUNTIME DESTINATION bin)

# ########## Paramount ##########
SET(paramount_SRC
    paramount.cpp)

add_executable(indi_paramount_telescope ${paramount_SRC})

target_link_libraries(indi_paramount_telescope indidriver)

install(TARGETS indi_paramount_telescope RUNTIME DESTINATION bin)

# ########## Astrotrac ##########
SET(astrotrac_SRC
    astrotrac.cpp)

add_executable(indi_astrotrac_telescope ${astrotrac_SRC})

target_link_libraries(indi_astrotrac_telescope indidriver AlignmentDriver)

install(TARGETS indi_astrotrac_telescope RUNTIME DESTINATION bin)

# ########## Syncscan ###############
SET(synscan_SRC
    synscandriver.cpp
    synscanmount.cpp)

add_executable(indi_synscan_telescope ${synscan_SRC})
target_link_libraries(indi_synscan_telescope indidriver AlignmentDriver)

install(TARGETS indi_synscan_telescope RUNTIME DESTINATION bin)

# ########## Syncscan Legacy ###############
SET(synscanlegacy_SRC
    synscandriverlegacy.cpp
    synscanmountlegacy.cpp)

add_executable(indi_synscanlegacy_telescope ${synscanlegacy_SRC})
target_link_libraries(indi_synscanlegacy_telescope indidriver AlignmentDriver)

install(TARGETS indi_synscanlegacy_telescope RUNTIME DESTINATION bin)

# ########## Sky Commander ###############
SET(skycommander_SRC
    skycommander.cpp)

add_executable(indi_skycommander_telescope ${skycommander_SRC})
target_link_libraries(indi_skycommander_telescope indidriver)

install(TARGETS indi_skycommander_telescope RUNTIME DESTINATION bin)

# ########## Generic Digital Setting Circle ###############
SET(dsc_SRC
    dsc.cpp)

add_executable(indi_dsc_telescope ${dsc_SRC})
target_link_libraries(indi_dsc_telescope indidriver AlignmentDriver)

install(TARGETS indi_dsc_telescope RUNTIME DESTINATION bin)

# ########## IEQ Pro Legacy #############
SET(ieqlegacy_SRC
    ieqprolegacydriver.cpp
    ieqprolegacy.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(ieqlegacy ieqlegacy_SRC 10 cpp)
ENDIF()

add_executable(indi_ieqlegacy_telescope ${ieqlegacy_SRC})

target_link_libraries(indi_ieqlegacy_telescope indidriver)

install(TARGETS indi_ieqlegacy_telescope RUNTIME DESTINATION bin)

# ########## IEQ Pro #############
SET(ieq_SRC
    ieqdriverbase.cpp
    ieqpro.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(ieq ieq_SRC 10 cpp)
ENDIF()

add_executable(indi_ieq_telescope ${ieq_SRC})

target_link_libraries(indi_ieq_telescope indidriver)

install(TARGETS indi_ieq_telescope RUNTIME DESTINATION bin)

# ########## IOptronV3 / CEM120 #############
SET(ioptronv3_SRC
    ioptronv3driver.cpp
    ioptronv3.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(ioptronv3 ioptronv3_SRC 10 cpp)
ENDIF()

add_executable(indi_ioptronv3_telescope ${ioptronv3_SRC})

target_link_libraries(indi_ioptronv3_telescope indidriver)

install(TARGETS indi_ioptronv3_telescope RUNTIME DESTINATION bin)

# ########## Explore Scientific PMC8 #############
SET(pmc8_SRC
    pmc8driver.cpp
    pmc8.cpp)

add_executable(indi_pmc8_telescope ${pmc8_SRC})

target_link_libraries(indi_pmc8_telescope indidriver)

install(TARGETS indi_pmc8_telescope RUNTIME DESTINATION bin)

# ########## Telescope Simulator ##############
SET(telescopesimulator_SRC
    telescope_simulator.cpp
    scopesim_helper.cpp)

add_executable(indi_simulator_telescope ${telescopesimulator_SRC})
target_link_libraries(indi_simulator_telescope indidriver)
install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin)

# ########## Telescope Scripting Gateway ##############
SET(telescopescript_SRC
    telescope_script.cpp)

add_executable(indi_script_telescope ${telescopescript_SRC})
target_link_libraries(indi_script_telescope indidriver)
install(TARGETS indi_script_telescope RUNTIME DESTINATION bin)
