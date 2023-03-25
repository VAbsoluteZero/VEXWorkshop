# -------------------------------------------------------------------------------------------------
# asserts & checks
cmake_minimum_required(VERSION 3.20)
if(${VEXGameOfLife_SOURCE_DIR} MATCHES "[\]\[]")
    message("Note: Path contains special characters and that could lead to CMAKE being broken.\n")
endif()
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
    message(FATAL_ERROR "Prevented top level (in-tree) build. Please specify build directory.\n")
endif()
# -------------------------------------------------------------------------------------------------
# initialization
# -------------------------------------------------------------------------------------------------
set (CMAKE_SYSTEM_VERSION 10.0 (latest installed version))
message("win: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
project(VEXGameOfLife VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_SUPPRESS_REGENERATION true) # no zero check  
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)
option(VEX_DEBUG_CMAKE "Enable cmake diagnostics" NO)  
option(VEX_FETCH "Enable fetching VEX" NO)

message("c compiler id : ${CMAKE_C_COMPILER_ID}")
message("cxx compiler id : ${CMAKE_CXX_COMPILER_ID}")
message("compiler path: ${CMAKE_CXX_COMPILER}") 
# include cmakemodules
include(FindGIT)  
include(FetchContent)   
find_package(Git QUIET)

set(VEX_ROOTDIR "${VEXGameOfLife_SOURCE_DIR}/vex/VEXlib")
set(VEX_INCLUDE_DIRS "${VEXGameOfLife_SOURCE_DIR}/vex/VEXlib")  
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P}) # Check for 64 or 32 bit

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_64 TRUE)
    set(PROCESSOR_ARCH "x64")
else()
    set(ARCH_64 FALSE)
    set(PROCESSOR_ARCH "x86") 
endif()  
 
if (MSVC_VERSION GREATER 1921 AND MSVC_TOOLSET_VERSION GREATER 141)
	set(MSVC_TOOLSET_VERSION 143)
    message(STATUS "MSVC_TOOLSET_VERSION set: ${MSVC_TOOLSET_VERSION}")
endif()
 
# -------------------------------------------------------------------------------------------------
# For now VEX is distributed as SRC as there isn't much to justify lib
# -------------------------------------------------------------------------------------------------
if(VEX_FETCH)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        # Update submodules 
        message(STATUS "fetching submodules:")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_VEX_FETCH_R)
        if(NOT GIT_VEX_FETCH_R EQUAL "0")
            message(FATAL_ERROR "vex fetch failed:${GIT_VEX_FETCH_R}")
        else()
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --remote --merge
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_VEX_FETCH_R)
        endif() 
    endif() 
endif()

message(STATUS 
    "warning: Currently only webgpu is supported. Repo is very much work-in-progress and rough.")

if(NOT EXISTS "${VEX_ROOTDIR}/VCore")
    message(FATAL_ERROR "Couldn't find VEX lib or fetch submodules. VEX_FETCH either off or failed.")
endif() 
# -------------------------------------------------------------------------------------------------
# SUBDIRECTORY (for every platfrom)  
# -------------------------------------------------------------------------------------------------
add_subdirectory("third_party/spdlog")   
add_subdirectory("third_party/glm")   
add_subdirectory("third_party/rapidobj")  
# -------------------------------------------------------------------------------------------------
# configure include paths & source files 
# -------------------------------------------------------------------------------------------------
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${VEX_INCLUDE_DIRS})
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/SDL2/include/") 
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/spdlog/include/")
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/glm/")
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/")  
include_directories("${VEXGameOfLife_SOURCE_DIR}/src")  
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party_bin") 
include_directories("${VEXGameOfLife_SOURCE_DIR}/src_platform") 
# -------------------------------------------------------------------------------------------------  
# PLATFORM   
if (VEX_USE_WGPU AND VEX_USE_DAWN) 
    message(FATAL_ERROR "invalid configuration, wgpu and dawn enabled at once")
endif() 
if (VEX_USE_WGPU OR VEX_USE_DAWN OR VEX_USE_EMSCRIPTEN)  
    add_compile_definitions(VEX_GFX_WEBGPU=1)
    if (VEX_USE_WGPU)    
        add_compile_definitions(VEX_GFX_WEBGPU_WGPU=1)
    elseif(VEX_USE_DAWN)
        add_compile_definitions(VEX_GFX_WEBGPU_DAWN=1)
    elseif(VEX_USE_EMSCRIPTEN)
        add_compile_definitions(VEX_GFX_WEBGPU_EMSC=1)
    endif()
    set(VEX_BACKEND_WEBGPU ON)
elseif(VEX_USE_DX11)
    add_compile_definitions(VEX_GFX_DX11=1) 
    set(VEX_BACKEND_DIRECTX ON)
else() 
    message(FATAL_ERROR "error : Invalid configuration. Gfx backend not selected") 
endif() 
 
if (VEX_USE_DAWN)     
    # FETCH dawn
    message(STATUS "info: BACKEND = DAWN where:${VEXGameOfLife_SOURCE_DIR}/third_party_bin/dawn/") 
    add_subdirectory("${VEXGameOfLife_SOURCE_DIR}/third_party_bin/dawn/") 
    include_directories(${CMAKE_BINARY_DIR}/deps/dawn-build/gen/include/dawn) 
    include_directories(${VEXGameOfLife_SOURCE_DIR}/third_party_bin/dawn/)  
    # get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)  
endif()

if (EMSCRIPTEN) 
    message(STATUS " info PLATFORM : EMSCRIPTEN")  
	file(GLOB_RECURSE VEX_PLATFORM_SRC
        CONFIGURE_DEPENDS 
        "${VEXGameOfLife_SOURCE_DIR}src_platform/webgpu/*.cpp")

    include_directories("${VEXGameOfLife_SOURCE_DIR}/src_platform/webgpu/") 
	set(VEX_PLATFORM_IMGUISRC "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_wgpu.cpp") 
	set(CMAKE_EXECUTABLE_SUFFIX ".html")  

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY") #-s ASYNCIFY -s STRICT=1
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
	# debug stuff commented out   
	#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s DISABLE_EXCEPTION_CATCHING=1")

	set(CMAKE_EXE_LINKER_FLAGS "-s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ASSERTIONS=1 -s WASM=1 -s USE_WEBGPU=1 -s USE_SDL=2 -s SINGLE_FILE=1 ")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ")	  
    string(CONCAT EM_EXTRA_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
        "-s NO_EXIT_RUNTIME=1 --shell-file ${CMAKE_CURRENT_LIST_DIR}/src_platform/emscripten/shell.html "
        "-s USE_SDL_IMAGE=1 -s USE_SDL_MIXER=1 -s ENVIRONMENT=web  -s TEXTDECODER=2 "
        "-s ABORTING_MALLOC=0 -s ALLOW_MEMORY_GROWTH=0 -s MALLOC=emmalloc --output_eol=linux" #-s before
    )
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EM_EXTRA_LINK_FLAGS}")
    message(STATUS "info : emscripten linker flags: ${CMAKE_EXE_LINKER_FLAGS}") 
elseif (WIN32)   
    set(SDL_CMAKE_DEBUG_POSTFIX "d"  CACHE STRING "Name suffix for debug builds")
    message(STATUS " info PLATFORM : WIN32/64")  
    set(SDL_SHARED TRUE)
    add_subdirectory("third_party/SDL2") 
    include_directories("${VEXGameOfLife_SOURCE_DIR}/src_platform/windows/")
endif() 
# ------------------------------------------------------------------------------------------------- 
if (VEX_BACKEND_WEBGPU) 
	file(GLOB_RECURSE VEX_PLATFORM_SRC
        CONFIGURE_DEPENDS 
        "${VEXGameOfLife_SOURCE_DIR}/src_platform/webgpu/render/*.cpp"
        "${VEXGameOfLife_SOURCE_DIR}/src_platform/webgpu/*.cpp")
    set(VEX_PLATFORM_IMGUISRC "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_wgpu.cpp")
elseif(VEX_BACKEND_DIRECTX) 
	file(GLOB_RECURSE VEX_PLATFORM_SRC
        CONFIGURE_DEPENDS 
        "${VEXGameOfLife_SOURCE_DIR}/src_platform/windows/render/*.cpp") 
    if (VEX_USE_DX11)  
        set(VEX_PLATFORM_IMGUISRC "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx11.cpp")
    else()
       message(FATAL_ERROR "error : dx12 not implemented yet") 
    endif() 
endif() 
# ------------------------------------------------------------------------------------------------- 
# rest of source files    
file(GLOB_RECURSE VEX_GOL_SRC 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/platform/windows/*.cpp" "${VEXGameOfLife_SOURCE_DIR}/src/*.h")

file(GLOB_RECURSE VEX_GOL_SRC 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/src/*.cpp" "${VEXGameOfLife_SOURCE_DIR}/src/*.h")

file(GLOB_RECURSE VEX_SRC 
    CONFIGURE_DEPENDS 
    "${VEX_ROOTDIR}/VCore/*.cpp" #"${VEX_ROOTDIR}/VCore/*.h"
    "${VEX_ROOTDIR}/VFramework/*.cpp" #"${VEX_ROOTDIR}/VLang/*.h"
    "${VEX_ROOTDIR}/*.cpp" #"${VEX_ROOTDIR}/*.h"
)

set(VEX_ALLSRC ${VEX_SRC})
list (APPEND VEX_ALLSRC ${VEX_GOL_SRC})
list (APPEND VEX_ALLSRC ${VEX_PLATFORM_SRC}) 

set(VEX_SRC_AND_FILES ${VEX_ALLSRC})
file(GLOB VEX_OTHER_FILES 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/.clang-format" "${VEX_ROOTDIR}/.clang-format"
    "${VEXGameOfLife_SOURCE_DIR}/*.txt" "${VEXGameOfLife_SOURCE_DIR}/*.json"
    "${VEXGameOfLife_SOURCE_DIR}/src/*.txt" "${VEXGameOfLife_SOURCE_DIR}/src/*.json" 
    "${VEXGameOfLife_SOURCE_DIR}/content/shaders/*.hlsl" 
    "${VEX_ROOTDIR}/*.txt" "${VEX_ROOTDIR}/*.hlsl" "${VEX_ROOTDIR}/*.json")

file(GLOB_RECURSE VEX_GLM_HEADERS 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/glm/*.h" )
  
# -------------------------------------------------------------------------------------------------
# add gear imgui
file(GLOB_RECURSE VEX_IMGUI_HEADERS 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/*.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/*.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/gizmo/*.h")

add_library(imgui STATIC
    # Main Imgui files
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui.cpp" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp"
    # addons ImGuizmo/ImCurveEdit 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/gizmo/ImGuizmo.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/gizmo/ImCurveEdit.cpp"
    # SDL2+dx11-specific files
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl.cpp" 
    ${VEX_PLATFORM_IMGUISRC}# ${VEX_IMGUI_HEADERS}
    )  
# Make SDL2 available to Imgui
target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
# imgui/backends/ contains the SDL implementation
target_include_directories(imgui PUBLIC 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/misc/" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/")  
# -------------------------------------------------------------------------------------------------
list (APPEND VEX_SRC_AND_FILES ${VEX_OTHER_FILES})
list (APPEND VEX_SRC_AND_FILES ${VEX_GLM_HEADERS})
list (APPEND VEX_SRC_AND_FILES ${VEX_IMGUI_HEADERS})
# better structure for generated proj files
foreach(SRC_ITEM IN ITEMS ${VEX_SRC_AND_FILES})
    get_filename_component(SRC_PATH "${SRC_ITEM}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" SRC_GROUPPATH "${SRC_PATH}")
    string(REPLACE "/" "\\" SRC_GROUPPATH "${SRC_GROUPPATH}")
    source_group("${SRC_GROUPPATH}" FILES "${SRC_ITEM}")
    if(VEX_DEBUG_CMAKE)
        message(">>: ${SRC_ITEM} | ${SRC_GROUPPATH}") 
    endif() 
endforeach() 
if(VEX_DEBUG_CMAKE)
    message("sdl: ${SDL2_INCLUDE_DIRS}") 
    message("discovered source files: ${VEX_GOL_SRC}") 
    message("VEX: ${VEX_SRC}") 
    message("all source files: ${VEX_ALLSRC}") 
endif()  
# -------------------------------------------------------------------------------------------------
# add to VEXGameOfLife target
# -------------------------------------------------------------------------------------------------
if (MSVC)
	set(ORIGINAL_CMAKE_LINK_LFAGS "${CMAKE_EXE_LINKER_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS ") 
endif()  
add_compile_definitions(GLM_FORCE_LEFT_HANDED=1 GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
add_compile_definitions(VEXCORE_DEFINE_STRHASH=1 FMT_EXCEPTIONS=0 SPDLOG_NO_EXCEPTIONS=1)

add_compile_definitions( SPDLOG_COMPILED_LIB)
set(EXECUTABLE_OUTPUT_PATH  ${VEXGameOfLife_SOURCE_DIR}/build/)  
######################################################################################################
add_executable(VEXGameOfLife ${VEX_ALLSRC}) # VEXGameOfLife target
# libs
target_link_libraries(VEXGameOfLife PRIVATE rapidobj::rapidobj)
target_link_libraries(VEXGameOfLife PRIVATE imgui) 
target_link_libraries(VEXGameOfLife PRIVATE spdlog) 

if (NOT VEX_USE_BIN_DIR)
    set_target_properties(VEXGameOfLife
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/VexGame"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/VexGame"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/VexGame"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build/VexGame/Dbg"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/VexGame/Rel"
    )
else() 
    set_target_properties(VEXGameOfLife
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build_out/VexGame"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build_out/VexGame"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build_out/VexGame"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build_out/VexGame/${CMAKE_BUILD_TYPE}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build_out/VexGame/${CMAKE_BUILD_TYPE}"
    )
endif()

target_precompile_headers(VEXGameOfLife PRIVATE 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/glm/glm_common.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/spdlog/include/spdlog/spdlog.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/spdlog/include/spdlog/common.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Utils/VUtilsBase.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Utils/CoreTemplates.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Utils/VMath.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Containers/Dict.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Containers/Union.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Containers/Stack.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VCore/Containers/Tuple.h"
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VFramework/VEXBase.h"
) 

if (VEX_USE_DX11) 
    check_include_file(d3d11.h HAS_D3D11) 
    if(NOT HAS_D3D11)
        message(FATAL_ERROR " trying to link agains dx11 libs but they were not found") 
    endif()
    target_link_libraries(VEXGameOfLife PRIVATE d3d11 dxgi) 
endif() 
if(NOT EMSCRIPTEN)
    target_link_libraries(VEXGameOfLife PRIVATE SDL2main SDL2-static)  
    if (VEX_USE_DAWN)      
        target_link_libraries(VEXGameOfLife PRIVATE webgpu) 
    elseif(VEX_USE_WGPU)
        message(STATUS " ===> linking to WGPU") 
        add_subdirectory("third_party_bin/wgpu") 
        target_link_libraries(VEXGameOfLife PRIVATE webgpu) 
        target_copy_webgpu_binaries(VEXGameOfLife) 
    endif()
endif()

target_sources(VEXGameOfLife PRIVATE app_manifest.manifest) 
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    if(MSVC)
        #add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)
        message(STATUS "MSVC_TOOLSET_VERSION: ${MSVC_TOOLSET_VERSION}")
        if(NOT VEX_MSVC_CLANG) 
            target_compile_options(VEXGameOfLife PUBLIC "/ZI")
            target_compile_options(VEXGameOfLife PUBLIC "/MP") 
        endif()
        target_link_options(VEXGameOfLife PUBLIC "/INCREMENTAL") 
    endif() 
endif()  
# ------------------------------------------------------------------------------------------------- 
if (NOT EMSCRIPTEN)
    # -------------------------------------------------------------------------------------------------
    # Test target
    # ------------------------------------------------------------------------------------------------- 
    file(GLOB_RECURSE VEX_TST_SRC 
        CONFIGURE_DEPENDS 
        "${VEXGameOfLife_SOURCE_DIR}/src_test/*.cpp" "${VEXGameOfLife_SOURCE_DIR}/src_test/*.h"
        "${VEXGameOfLife_SOURCE_DIR}/src_test/vex/*.cpp"
    ) 
   
    set(SNITCH_DEFINE_MAIN OFF) 
    add_subdirectory("third_party/snitch")
    set(CMAKE_EXE_LINKER_FLAGS "${ORIGINAL_CMAKE_LINK_LFAGS}")
    add_executable(VexTests ${VEX_TST_SRC}  ${VEX_SRC}) # <=============== VexTests target
    target_link_libraries(VexTests PRIVATE snitch::snitch spdlog::spdlog)
    # -------------------------------------------------------------------------------------------------
    # Bench target
    # -------------------------------------------------------------------------------------------------
    file(GLOB_RECURSE VEX_BENCH_SRC 
        CONFIGURE_DEPENDS 
        "${VEXGameOfLife_SOURCE_DIR}/src_bench/*.cpp" "${VEXGameOfLife_SOURCE_DIR}/src_bench/*.h"
        "${VEXGameOfLife_SOURCE_DIR}/src_bench/vex/*.cpp"
    ) 

    add_executable(VexBench ${VEX_BENCH_SRC} ${VEX_SRC} "src_bench/misc/bittests.cpp")  # <=============== VexBench target
    target_link_libraries(VexBench PRIVATE snitch::snitch spdlog::spdlog)
    set_target_properties(VexBench
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bench/"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bench/"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bench/"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build/bench/${CMAKE_BUILD_TYPE}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/bench/${CMAKE_BUILD_TYPE}"
    )  
    # POST BUILD 
    add_custom_command ( TARGET VEXGameOfLife POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
         $<TARGET_FILE:SDL2> $<TARGET_FILE_DIR:VEXGameOfLife>
    )  
    add_custom_command(TARGET VEXGameOfLife POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/content/ $<TARGET_FILE_DIR:VEXGameOfLife>/content/) 
endif() 
# -------------------------------------------------------------------------------------------------
# extras 
if (MSVC)
	target_compile_options(VEXGameOfLife PRIVATE /W2)
endif() 
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") 
    target_compile_options(VEXGameOfLife PRIVATE -Wno-class-conversion)
endif() 
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "  include dirs:  ")
foreach(dir ${dirs})
    message(STATUS " '${dir}' ")
endforeach()