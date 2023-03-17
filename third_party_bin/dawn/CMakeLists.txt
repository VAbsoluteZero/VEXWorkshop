add_library(webgpu INTERFACE)

if (EMSCRIPTEN)

	target_include_directories(webgpu INTERFACE
		"${CMAKE_CURRENT_SOURCE_DIR}/include-emscripten"
	)

	# This is used to advertise the flavor of WebGPU that this zip provides
	target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_EMSCRIPTEN)

else (EMSCRIPTEN)

	include(cmake/FetchDawn.cmake) 
	make_dawn_available()

	target_link_libraries(webgpu INTERFACE webgpu_dawn libtint)
	target_include_directories(webgpu INTERFACE
		"${CMAKE_BINARY_DIR}/_deps/dawn-src/include"
	)

	# This is used to advertise the flavor of WebGPU that this zip provides
	target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_DAWN) 
endif (EMSCRIPTEN)
 
function(target_copy_webgpu_binaries Target)
endfunction()
