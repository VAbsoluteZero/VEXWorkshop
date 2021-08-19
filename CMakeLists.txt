# -------------------------------------------------------------------------------------------------
# asserts & checks
# -------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)
if(${VEXGameOfLife_SOURCE_DIR} MATCHES "[\]\[]")
    message("Note: Path contains special characters and that could lead to CMAKE being broken.\n")
endif()
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
    message(FATAL_ERROR "Prevented top level (in-tree) build. Please specify build directory.\n")
endif()
# -------------------------------------------------------------------------------------------------
# initialization
# -------------------------------------------------------------------------------------------------
project(VEXGameOfLife VERSION 0.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(VEX_DEBUG_CMAKE "Enable cmake diagnostics" NO)
option(VEX_ADDSDL "Enable building SDL" YES)
option(VEX_FETCH "Enable fetching VEX" YES)

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

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/repo/CMakeLists.txt")
    message(FATAL_ERROR "Couldn't fetch submodules. VEX_FETCH either off or failed.")
endif() 

if(VEX_ADDSDL)
    add_subdirectory("third_party/SDL2")
    add_subdirectory("third_party/spdlog") 
endif()
# -------------------------------------------------------------------------------------------------
# configure include paths & source files
# -------------------------------------------------------------------------------------------------
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${VEX_INCLUDE_DIRS})
include_directories(${VEXGameOfLife_SOURCE_DIR}) 
# ------------------------------------------------------------------------------------------------- 
file(GLOB_RECURSE VEX_GOL_SRC 
    CONFIGURE_DEPENDS 
    "${VEXGameOfLife_SOURCE_DIR}/src/*.cpp")

file(GLOB VEX_GOL_SRC 
    CONFIGURE_DEPENDS 
    "${VEX_ROOTDIR}/VCore/*.cpp"
    "${VEX_ROOTDIR}/VLang/*.cpp"
    "${VEX_ROOTDIR}/*.cpp")
# -------------------------------------------------------------------------------------------------
# debug log
# -------------------------------------------------------------------------------------------------
if(VEX_DEBUG_CMAKE)
    message("discovered source files: ${VEX_GOL_SRC}") 
    message("sdl: ${SDL2_INCLUDE_DIRS}") 
    message("VEX: ${VEX_SRC}") 
endif() 

# -------------------------------------------------------------------------------------------------
# add to targets
# ------------------------------------------------------------------------------------------------- 
add_executable(VEXGameOfLife ${VEX_GOL_SRC} ${VEX_SRC})
target_link_libraries(VEXGameOfLife PRIVATE SDL2main SDL2-static)

