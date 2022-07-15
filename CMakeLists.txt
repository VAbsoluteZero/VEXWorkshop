# -------------------------------------------------------------------------------------------------
# asserts & checks
# -------------------------------------------------------------------------------------------------
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
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

option(VEX_DEBUG_CMAKE "Enable cmake diagnostics" NO) 
option(VEX_FETCH "Enable fetching VEX" NO)

# include cmakemodules
include(FindGIT) 
include(FetchContent) 
find_package(Git QUIET)
 
# set variables 
set(CMAKE_SUPPRESS_REGENERATION true) # no zero check 
set(RUNTIME_OUTPUT_DIRECTORY "build/bin/exec")
set(LIBRARY_OUTPUT_DIRECTORY "build/bin/libs")
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

if(VEX_DEBUG_CMAKE)
    message("Invoked in directory: ${VEXGameOfLife_SOURCE_DIR}")
endif()
 
if (MSVC_VERSION GREATER 1921 AND MSVC_TOOLSET_VERSION GREATER 141)
	set(MSVC_TOOLSET_VERSION 143)
    message(STATUS "MSVC_TOOLSET_VERSION set: ${MSVC_TOOLSET_VERSION}")
endif()

set(SDL_CMAKE_DEBUG_POSTFIX "d"
    CACHE STRING "Name suffix for debug builds")

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
        endif() 
    endif() 
endif()

if(NOT EXISTS "${VEX_ROOTDIR}/.git")
    message(FATAL_ERROR "Couldn't fetch submodules. VEX_FETCH either off or failed.")
endif() 

add_subdirectory("third_party/SDL2")
add_subdirectory("third_party/spdlog")  
add_subdirectory("third_party/glm")  
# -------------------------------------------------------------------------------------------------
# configure include paths & source files
# -------------------------------------------------------------------------------------------------
include_directories(${SDL2_INCLUDE_DIRS})
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/SDL2/include/")
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/spdlog/include/")
include_directories("${VEXGameOfLife_SOURCE_DIR}/third_party/glm/")
include_directories(${VEX_INCLUDE_DIRS})
include_directories(${VEXGameOfLife_SOURCE_DIR}/src) 
# ------------------------------------------------------------------------------------------------- 
file(GLOB_RECURSE VEX_GOL_SRC 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/src/*.cpp" "${VEXGameOfLife_SOURCE_DIR}/src/*.h")

file(GLOB_RECURSE VEX_SRC 
    CONFIGURE_DEPENDS 
    "${VEX_ROOTDIR}/VCore/*.cpp" "${VEX_ROOTDIR}/VCore/*.h"
    "${VEX_ROOTDIR}/VFramework/*.cpp" "${VEX_ROOTDIR}/VLang/*.h"
    "${VEX_ROOTDIR}/*.cpp" "${VEX_ROOTDIR}/*.h")

set(VEX_ALLSRC ${VEX_SRC})
list (APPEND VEX_ALLSRC ${VEX_GOL_SRC})

set(VEX_SRC_AND_FILES ${VEX_ALLSRC})
file(GLOB VEX_OTHER_FILES 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/.clang-format" "${VEX_ROOTDIR}/.clang-format"
    "${VEXGameOfLife_SOURCE_DIR}/*.txt" "${VEXGameOfLife_SOURCE_DIR}/*.json"
    "${VEXGameOfLife_SOURCE_DIR}/src/*.txt" "${VEXGameOfLife_SOURCE_DIR}/src/*.json"
    "${VEX_ROOTDIR}/*.txt" "${VEX_ROOTDIR}/*.json")

file(GLOB_RECURSE VEX_GLM_HEADERS 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/glm/*.h" )
  
# -------------------------------------------------------------------------------------------------
# add gear imgui
file(GLOB_RECURSE VEX_IMGUI_HEADERS 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/*.h"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/*.h")

add_library(imgui STATIC
    # Main Imgui files
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui.cpp" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp"
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp"
    # SDL2+OpenGL-specific files
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl.cpp" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx11.cpp" ${VEX_IMGUI_HEADERS})
     
    
source_group("Headers" FILES "${SRC_ITEM}")

# Make SDL2 available to Imgui
target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
# imgui/backends/ contains the SDL implementation
target_include_directories(imgui PUBLIC 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/misc/" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/" 
    "${VEXGameOfLife_SOURCE_DIR}/third_party/imgui/backends/")
# Configure Imgui to use OpengGL through SDL2
# target_compile_definitions(imgui PUBLIC  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# -------------------------------------------------------------------------------------------------

list (APPEND VEX_SRC_AND_FILES ${VEX_OTHER_FILES})
list (APPEND VEX_SRC_AND_FILES ${VEX_GLM_HEADERS})
list (APPEND VEX_SRC_AND_FILES ${VEX_IMGUI_HEADERS})
 
foreach(SRC_ITEM IN ITEMS ${VEX_SRC_AND_FILES})
    get_filename_component(SRC_PATH "${SRC_ITEM}" PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}" "" SRC_GROUPPATH "${SRC_PATH}")
    string(REPLACE "/" "\\" SRC_GROUPPATH "${SRC_GROUPPATH}")
    source_group("${SRC_GROUPPATH}" FILES "${SRC_ITEM}")
    message(">>: ${SRC_ITEM} | ${SRC_GROUPPATH}") 
endforeach()
# -------------------------------------------------------------------------------------------------
# debug log
# -------------------------------------------------------------------------------------------------

if(VEX_DEBUG_CMAKE)
    message("sdl: ${SDL2_INCLUDE_DIRS}") 
    message("discovered source files: ${VEX_GOL_SRC}") 
    message("VEX: ${VEX_SRC}") 
    message("all source files: ${VEX_ALLSRC}") 
endif() 

message("c compiler id : ${CMAKE_C_COMPILER_ID}")
message("cxx compiler id : ${CMAKE_CXX_COMPILER_ID}")
message("compiler path: ${CMAKE_CXX_COMPILER}")

set(VEX_MSVC_CLANG NO)
if (MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS ")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "(dbg) cpp compiler identified : CLANG-CL")
	    set(VEX_MSVC_CLANG YES)
    endif()
endif()
# manifest

# -------------------------------------------------------------------------------------------------
# add to targets
# -------------------------------------------------------------------------------------------------
set(EXECUTABLE_OUTPUT_PATH  ${VEXGameOfLife_SOURCE_DIR}/build/) 

add_executable(VEXGameOfLife ${VEX_ALLSRC})

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
    "${VEXGameOfLife_SOURCE_DIR}/vex/VEXLib/VFramework/VEXBase.h")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    if(MSVC)
        message(STATUS "MSVC_TOOLSET_VERSION: ${MSVC_TOOLSET_VERSION}")
        if(NOT VEX_MSVC_CLANG)
            target_compile_options(VEXGameOfLife PUBLIC "/ZI")
            target_compile_options(VEXGameOfLife PUBLIC "/MP")
            target_link_options(VEXGameOfLife PUBLIC "/INCREMENTAL")  
        endif()
    endif()
endif() 

target_link_libraries(VEXGameOfLife PRIVATE imgui)
target_link_libraries(VEXGameOfLife PRIVATE SDL2main SDL2-static) 


target_sources(VEXGameOfLife PRIVATE app_manifest.manifest)

add_custom_command(TARGET VEXGameOfLife POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/content/ $<TARGET_FILE_DIR:VEXGameOfLife>/content/)

add_custom_command ( TARGET VEXGameOfLife POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2> $<TARGET_FILE_DIR:VEXGameOfLife>-
)

check_include_file(d3d11.h HAS_D3D11)
if (HAS_D3D11)
    message (STATUS "  DirectX 11  found.")
else ()
    message (STATUS "  DirectX 11 could not be found.")
endif ()

target_link_libraries(VEXGameOfLife PRIVATE d3d11)

  