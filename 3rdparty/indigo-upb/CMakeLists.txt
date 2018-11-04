cmake_minimum_required(VERSION 2.7)
project(indigo_aux_upb)

if (UNIX AND NOT APPLE)

	execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCH)

	message("Architecture " ${ARCH})

	if (${ARCH} MATCHES "armv7l")
		exec_program(cp ARGS armhf.bin indigo_aux_upb)
	endif ()

	if (${ARCH} MATCHES "aarch64")
		exec_program(cp ARGS arm64.bin indigo_aux_upb)
	endif ()

	if (${ARCH} MATCHES "i686")
		exec_program(cp ARGS i386.bin indigo_aux_upb)
	endif ()

	if (${ARCH} MATCHES "x86_64")
		exec_program(cp ARGS amd64.bin indigo_aux_upb)
	endif ()

	install(FILES indigo_aux_upb DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	install(FILES 99-indigo_aux_upb.rules DESTINATION /lib/udev/rules.d)
	install(FILES indi_upb.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/indi)

else ()

	message("Unsupported platform, try INDIGO project repository instead")

endif ()

