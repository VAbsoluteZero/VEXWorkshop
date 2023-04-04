include(FetchContent)

set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/deps/")

FetchContent_Declare(
	dawn
	GIT_REPOSITORY https://dawn.googlesource.com/dawn
	GIT_TAG        chromium/5644
	GIT_SUBMODULES
)

function(make_dawn_available)
	#set(SAVED_TG_BUILD_TYPE CMAKE_BUILD_TYPE)
	#set(CMAKE_BUILD_TYPE "RelWithDebInfo")
	message(STATUS "fetch_dawn_dependencies: ${SAVED_TG_BUILD_TYPE}")
	FetchContent_GetProperties(dawn)
	if (NOT dawn_POPULATED)
		FetchContent_Populate(dawn)
		
		message(STATUS " running fetch script")
		execute_process(
			COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/fetch_dawn_dependencies.py"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/deps/dawn-src/"
			OUTPUT_VARIABLE OUT1
			RESULT_VARIABLE OUT2
			ERROR_VARIABLE OUT3
		)
		message(STATUS "fetch dawn dependencies: ${OUT1}")
		message(STATUS " w ${OUT2}")
		message(STATUS " e  ${OUT3}")
		message(STATUS "dir: ${CMAKE_BINARY_DIR}/deps/dawn-src" )

		set(DAWN_BUILD_SAMPLES OFF)
		set(TINT_BUILD_TINT ON)  
		set(TINT_BUILD_SAMPLES OFF)
		set(TINT_BUILD_DOCS OFF)
		set(TINT_BUILD_TESTS OFF)

		set(DAWN_ENABLE_D3D12 ON)
		set(DAWN_ENABLE_METAL OFF)
		set(DAWN_ENABLE_NULL ON)
		set(DAWN_ENABLE_DESKTOP_GL OFF)
		set(DAWN_ENABLE_OPENGLES OFF)
		set(DAWN_ENABLE_VULKAN OFF)

		set(TINT_BUILD_SPV_READER ON)
		set(TINT_BUILD_WGSL_READER ON)
		set(TINT_BUILD_GLSL_WRITER OFF)
		set(TINT_BUILD_HLSL_WRITER ON)
		set(TINT_BUILD_MSL_WRITER OFF)
		set(TINT_BUILD_SPV_WRITER ON)
		set(TINT_BUILD_WGSL_WRITER ON) # Because of crbug.com/dawn/1481
		set(TINT_BUILD_FUZZERS OFF)
		set(TINT_BUILD_SPIRV_TOOLS_FUZZER OFF)
		set(TINT_BUILD_AST_FUZZER OFF)
		set(TINT_BUILD_REGEX_FUZZER OFF)
		set(TINT_BUILD_BENCHMARKS OFF)
		set(TINT_BUILD_TESTS OFF)
		set(TINT_BUILD_AS_OTHER_OS OFF)
		set(TINT_BUILD_REMOTE_COMPILE OFF)

		message(STATUS "================================================================")
		message(STATUS ${dawn_SOURCE_DIR} "  :: info :: " ${dawn_BINARY_DIR})
		add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR})
	endif ()

	set(AllDawnTargets
		core_tables
		dawn_common
	#	dawn_glfw
		dawn_headers
		dawn_native
		dawn_platform
		dawn_proc
		dawn_utils
		dawn_wire
		dawncpp
		dawncpp_headers
		emscripten_bits_gen
		enum_string_mapping
		extinst_tables
		webgpu_dawn
		webgpu_headers_gen
		dawn_public_config
	)
	 
	foreach (Target ${AllDawnTargets})
		set_property(TARGET ${Target} PROPERTY FOLDER "Dawn")
		set_property(TARGET ${Target} PROPERTY COMPILE_WARNING_AS_ERROR NO) 
	endforeach()
	# set_property(TARGET glfw PROPERTY FOLDER "External/GLFW3")
	# set_property(TARGET update_mappings PROPERTY FOLDER "External/GLFW3")

	# This is likely needed for other targets as well dawn_public_config
	target_include_directories(dawn_utils PUBLIC "${CMAKE_BINARY_DIR}/deps/dawn-src/src")
	#set(CMAKE_BUILD_TYPE SAVED_TG_BUILD_TYPE)
endfunction()
