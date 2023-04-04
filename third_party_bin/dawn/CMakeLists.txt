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

	target_link_libraries(webgpu INTERFACE webgpu_dawn dawn_public_config libtint)
	target_include_directories(webgpu INTERFACE
		"${CMAKE_BINARY_DIR}/deps/dawn-src/include"
	)
  if (MSVC)
	target_compile_options(libtint PRIVATE
      /bigobj
      /EHsc
      /W4 
      /wd4068
      /wd4127
      /wd4244
      /wd4267
      /wd4324
      /wd4458
      /wd4514
      /wd4571
      /wd4625
      /wd4626
      /wd4710
      /wd4774
      /wd4820
      /wd5026
      /wd5027 
    )
    endif(MSVC)
	# This is used to advertise the flavor of WebGPU that this zip provides
	target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_DAWN) 
endif (EMSCRIPTEN)
 
function(target_copy_webgpu_binaries Target)
endfunction()
