cmake_minimum_required(VERSION 3.20)

add_library(rapidobj INTERFACE)
add_library(rapidobj::rapidobj ALIAS rapidobj)

target_compile_features(rapidobj INTERFACE cxx_std_17)

target_include_directories(rapidobj INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${RAPIDOBJ_INCLUDE_DIR}>
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
    find_package(Aio REQUIRED)
    target_link_libraries(rapidobj INTERFACE Threads::Threads aio)
    target_link_libraries(rapidobj INTERFACE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)
endif()

# write_basic_package_version_file(RapidObjConfigVersion.cmake VERSION ${VERSION} COMPATIBILITY AnyNewerVersion)

set(DESTINATION_CMAKE_DIR "${RAPIDOBJ_CMAKE_DIR}/RapidObj")
set(DESTINATION_INCLUDE_DIR "${RAPIDOBJ_INCLUDE_DIR}/rapidobj") 

install(TARGETS rapidobj EXPORT RapidObj)
install(EXPORT RapidObj DESTINATION ${DESTINATION_CMAKE_DIR} NAMESPACE rapidobj::)
install(FILES ${PROJECT_SOURCE_DIR}/cmake/RapidObjConfig.cmake DESTINATION ${DESTINATION_CMAKE_DIR})
install(FILES ${PROJECT_BINARY_DIR}/include/RapidObjConfigVersion.cmake DESTINATION ${DESTINATION_CMAKE_DIR})
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(FILES ${PROJECT_SOURCE_DIR}/cmake/FindAio.cmake DESTINATION ${DESTINATION_CMAKE_DIR})

    # pkg-config file
    configure_file(${PROJECT_NAME}.pc.in
        ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc
        @ONLY
    )
    install(FILES
        ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
endif() 

install(FILES rapidobj/rapidobj.hpp DESTINATION ${DESTINATION_INCLUDE_DIR})
