# ########## getINDI ##############
add_executable(indi_getprop getINDIproperty.c)

target_link_libraries(indi_getprop indicore eventloop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getprop RUNTIME DESTINATION bin)

# ########## setINDI ##############
add_executable(indi_setprop setINDIproperty.c)

target_link_libraries(indi_setprop indicore eventloop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_setprop RUNTIME DESTINATION bin)

# ########## evalINDI ##############
add_executable(indi_eval compiler.c evalINDI.c)

target_link_libraries(indi_eval indicore eventloop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_eval RUNTIME DESTINATION bin)

# ########## getINDIdevice ##############
add_executable(indi_getdevice getINDIdevice.c)

target_link_libraries(indi_getdevice indicore eventloop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getdevice RUNTIME DESTINATION bin)
