# ########## getINDI ##############
add_executable(indi_getprop
    getINDIproperty.c
    ../sharedblob.c
    ../libs/lilxml.cpp
    ../libs/userio.c
    ../libs/indiuserio.c
    ../libs/indicom.c
    ../libs/libastro.cpp)

target_link_libraries(indi_getprop eventloop base64 ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getprop RUNTIME DESTINATION bin)

# ########## setINDI ##############
add_executable(indi_setprop
    setINDIproperty.c
    ../sharedblob.c
    ../libs/lilxml.cpp
    ../libs/userio.c
    ../libs/indiuserio.c
    ../libs/indicom.c
    ../libs/libastro.cpp)

target_link_libraries(indi_setprop eventloop base64 ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_setprop RUNTIME DESTINATION bin)

# ########## evalINDI ##############
add_executable(indi_eval
    compiler.c
    evalINDI.c
    ../sharedblob.c
    ../libs/lilxml.cpp
    ../libs/userio.c
    ../libs/indiuserio.c
    ../libs/indicom.c
    ../libs/libastro.cpp)

target_link_libraries(indi_eval eventloop base64 ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_eval RUNTIME DESTINATION bin)
