# ###################################################################################################
#
# Component   : LX200 Library
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
# ################################################################################################

add_library(indilx200 SHARED
    lx200driver.cpp
    lx200telescope.cpp)

if(NOT WIN32)
    set_target_properties(indilx200 PROPERTIES COMPILE_FLAGS "-fPIC")
endif(NOT WIN32)

target_compile_definitions(indilx200 PRIVATE _XOPEN_SOURCE=700 HAVE_LIBNOVA)
set_target_properties(indilx200 PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indilx200)

target_link_libraries(indilx200 indidevice $<$<PLATFORM_ID:CYGWIN>:indidriver>)

install(TARGETS indilx200 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    lx200telescope.h
    lx200driver.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/mounts
    COMPONENT Devel)

# ################################################################################

# ########## LX200 Basic #############
add_executable(indi_lx200basic
    lx200driver.cpp
    lx200basic.cpp)

target_link_libraries(indi_lx200basic indidriver)

install(TARGETS indi_lx200basic RUNTIME DESTINATION bin)

# ################################################################################

# ########## LX200 TeenAstro #############
add_executable(indi_lx200_TeenAstro
    lx200driver.cpp
    lx200_TeenAstro.cpp)

target_compile_definitions(indi_lx200_TeenAstro PRIVATE _XOPEN_SOURCE=700)
target_link_libraries(indi_lx200_TeenAstro indidriver)

install(TARGETS indi_lx200_TeenAstro RUNTIME DESTINATION bin)

# ################################################################################

# ########## LX200 Generic ###########
add_executable(indi_lx200generic
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
    lx200ap_v2.cpp
    lx200ap_gtocp2.cpp
    lx200fs2.cpp
    lx200ss2000pc.cpp
    lx200_OnStep.cpp
    lx200_OpenAstroTech.cpp
    lx200_pegasus_nyx101.cpp
    lx200_10micron.cpp
    ioptronHC8406.cpp
    eq500x.cpp)

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
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_v2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_legacy)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gemini)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200zeq25)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200am5)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gotonova)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200pulsar2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ss2000pc)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OnStep)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OpenAstroTech)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_pegasus_nyx101)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_10micron)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_ioptronHC8406)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_eq500x_telescope)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)

# ################################################################################

# ########## Celestron GPS ############
add_executable(indi_celestron_gps
    celestrondriver.cpp
    celestrongps.cpp)

target_link_libraries(indi_celestron_gps indidriver)

install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin)

# ########## Rainbow Mount driver ##########
add_executable(indi_rainbow_telescope
    rainbow.cpp
    lx200driver.cpp)

target_compile_definitions(indi_rainbow_telescope PRIVATE _XOPEN_SOURCE=700)

target_link_libraries(indi_rainbow_telescope indidriver)

install(TARGETS indi_rainbow_telescope RUNTIME DESTINATION bin)

# ########## HOBYM / TitanTCS for CRUX Mount ##########
add_executable(indi_crux_mount
    crux_mount.cpp)

target_link_libraries(indi_crux_mount indidriver)

install(TARGETS indi_crux_mount RUNTIME DESTINATION bin)

# ########## Takahashi Temma ##########
add_executable(indi_temma_telescope
    temmadriver.cpp)

target_link_libraries(indi_temma_telescope indidriver AlignmentDriver)

install(TARGETS indi_temma_telescope RUNTIME DESTINATION bin)

# ########## Paramount ##########
add_executable(indi_paramount_telescope
    paramount.cpp)

target_link_libraries(indi_paramount_telescope indidriver)

install(TARGETS indi_paramount_telescope RUNTIME DESTINATION bin)

# ########## Astrotrac ##########
add_executable(indi_astrotrac_telescope
    astrotrac.cpp)

target_link_libraries(indi_astrotrac_telescope AlignmentDriver indidriver)

install(TARGETS indi_astrotrac_telescope RUNTIME DESTINATION bin)

# ########## Syncscan ###############
add_executable(indi_synscan_telescope
    synscandriver.cpp
    synscanmount.cpp)

target_link_libraries(indi_synscan_telescope indidriver AlignmentDriver)

install(TARGETS indi_synscan_telescope RUNTIME DESTINATION bin)

# ########## Syncscan Legacy ###############
add_executable(indi_synscanlegacy_telescope
    synscandriverlegacy.cpp
    synscanmountlegacy.cpp)

target_link_libraries(indi_synscanlegacy_telescope indidriver AlignmentDriver)

install(TARGETS indi_synscanlegacy_telescope RUNTIME DESTINATION bin)

# ########## Sky Commander ###############
add_executable(indi_skycommander_telescope
    skycommander.cpp)

target_link_libraries(indi_skycommander_telescope indidriver)

install(TARGETS indi_skycommander_telescope RUNTIME DESTINATION bin)

# ########## Generic Digital Setting Circle ###############
add_executable(indi_dsc_telescope
    dsc.cpp)

target_link_libraries(indi_dsc_telescope AlignmentDriver indidriver)

install(TARGETS indi_dsc_telescope RUNTIME DESTINATION bin)

# ########## IEQ Pro Legacy #############
add_executable(indi_ieqlegacy_telescope
    ieqprolegacydriver.cpp
    ieqprolegacy.cpp)

target_link_libraries(indi_ieqlegacy_telescope indidriver)

install(TARGETS indi_ieqlegacy_telescope RUNTIME DESTINATION bin)

# ########## IEQ Pro #############
add_executable(indi_ieq_telescope
    ieqdriverbase.cpp
    ieqpro.cpp)

target_link_libraries(indi_ieq_telescope indidriver)

install(TARGETS indi_ieq_telescope RUNTIME DESTINATION bin)

# ########## IOptronV3 / CEM120 #############
add_executable(indi_ioptronv3_telescope
    ioptronv3driver.cpp
    ioptronv3.cpp)

target_link_libraries(indi_ioptronv3_telescope indidriver)

install(TARGETS indi_ioptronv3_telescope RUNTIME DESTINATION bin)

# ########## Explore Scientific PMC8 #############
add_executable(indi_pmc8_telescope
    pmc8driver.cpp
    pmc8.cpp)

target_link_libraries(indi_pmc8_telescope indidriver)

install(TARGETS indi_pmc8_telescope RUNTIME DESTINATION bin)

# ########## Telescope Simulator ##############
add_executable(indi_simulator_telescope
    telescope_simulator.cpp
    scopesim_helper.cpp)

target_link_libraries(indi_simulator_telescope indidriver)

install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin)

# ########## Telescope Scripting Gateway ##############
add_executable(indi_script_telescope
    telescope_script.cpp)

target_link_libraries(indi_script_telescope indidriver)

install(TARGETS indi_script_telescope RUNTIME DESTINATION bin)

# ########## Skywatcher Alt-Az Mount ##############
add_executable(indi_skywatcherAltAzMount
    skywatcherAPIMount.cpp
    skywatcherAPI.cpp)

target_link_libraries(indi_skywatcherAltAzMount AlignmentDriver indidriver)

install(TARGETS indi_skywatcherAltAzMount RUNTIME DESTINATION bin)

# ########## Planewave Mount ##############
add_executable(indi_planewave_telescope
    planewave_mount.cpp)

target_link_libraries(indi_planewave_telescope indidriver ${HTTPLIB_LIBRARY})

install(TARGETS indi_planewave_telescope RUNTIME DESTINATION bin)
