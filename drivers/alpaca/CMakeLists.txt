########### Alpaca Server ###########
set(indi_alpaca_SRCS
    indi_alpaca_server.cpp
    alpaca_client.cpp
    device_manager.cpp
    alpaca_discovery.cpp
    bridges/telescope_bridge_base.cpp
    bridges/telescope_bridge_handlers.cpp
    bridges/camera_bridge_base.cpp
    bridges/camera_bridge_handlers.cpp
)

add_executable(indi_alpaca_server ${indi_alpaca_SRCS})
target_link_libraries(indi_alpaca_server indidriver indiclient ${HTTPLIB_LIBRARY} Threads::Threads)

install(TARGETS indi_alpaca_server RUNTIME DESTINATION bin)
