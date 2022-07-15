cmake_minimum_required(VERSION 3.9)

# define configurations
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES Debug Release)
    set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}/)
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/)
else()
    set(CMAKE_BUILD_TYPE Release)
    set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib/)
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/)
endif()

project(ssdp-connect C)
add_library(ssdp-connect STATIC ssdp.h ssdp.c ssdp-connect.h ssdp-connect.c)

# set output directories
set_target_properties(ssdp-connect PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${LIB_DIR})

option(BUILD_EXAMPLE "Build usage example" OFF)
if(BUILD_EXAMPLE)
	add_executable(ssdp-server-example example/ssdp-server-example.c)
	target_link_libraries(ssdp-server-example ssdp-connect)
	set_target_properties(ssdp-server-example PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
	
	add_executable(ssdp-client-example example/ssdp-client-example.c)
	target_link_libraries(ssdp-client-example ssdp-connect)
	set_target_properties(ssdp-client-example PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
	
	if (WIN32)
		target_link_libraries(ssdp-client-example Ws2_32.lib)
		target_link_libraries(ssdp-server-example Ws2_32.lib)
	endif()
endif()
