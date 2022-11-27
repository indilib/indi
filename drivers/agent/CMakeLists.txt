# ########## Imager ##############
set(imager_SRCS
    agent_imager.cpp
    group.cpp
)

add_executable(indi_imager_agent ${imager_SRCS})
target_link_libraries(indi_imager_agent indidriver indiclient)
install(TARGETS indi_imager_agent RUNTIME DESTINATION bin)
