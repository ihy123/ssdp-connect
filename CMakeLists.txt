cmake_minimum_required(VERSION 3.9)

# define configurations
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES Debug Release)
else()
    set(CMAKE_BUILD_TYPE Release)
endif()

project(ssdp-discovery C)
add_library(ssdp-discovery STATIC ssdp.h ssdp.c)

# set output directories
set_target_properties(ssdp-discovery PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
