set(WGPU ${CMAKE_CURRENT_SOURCE_DIR})
if (NOT ARCH)
	set(ARCH x86_64)
endif()
 
add_library(webgpu SHARED IMPORTED GLOBAL)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")

	set(WGPU_RUNTIME_LIB ${WGPU}/windows-${ARCH}/wgpu_native.dll PARENT_SCOPE)
	set_target_properties(
		webgpu
		PROPERTIES
			IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
			IMPORTED_IMPLIB "${WGPU}/windows-${ARCH}/wgpu_native.lib"
			INTERFACE_INCLUDE_DIRECTORIES "${WGPU}"
	)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

	set(WGPU_RUNTIME_LIB ${WGPU}/linux-${ARCH}/libwgpu.so PARENT_SCOPE)
	set_target_properties(
		webgpu
		PROPERTIES
			IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
			INTERFACE_INCLUDE_DIRECTORIES "${WGPU}"
	) 

else()

	message(FATAL_ERROR "WebGPU lib has no binaries for this platform.")

endif()
 
function(target_copy_webgpu_binaries Target)
	add_custom_command(
		TARGET ${Target} POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different
			${WGPU_RUNTIME_LIB}
			$<TARGET_FILE_DIR:${Target}>
	)
endfunction()
