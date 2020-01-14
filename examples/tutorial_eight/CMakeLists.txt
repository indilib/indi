########### Tutorial Eight ##############
if (CFITSIO_FOUND)
add_executable(tutorial_eight simple_spectrograph.cpp)

target_link_libraries(tutorial_eight indidriver)
endif (CFITSIO_FOUND)


