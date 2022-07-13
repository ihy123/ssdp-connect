cmake_minimum_required(VERSION 3.9)

# define configurations
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES Debug Release)
    set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}/)
else()
    set(CMAKE_BUILD_TYPE Release)
    set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/lib/)
endif()

project(ssdp-connect C)
add_library(ssdp-connect STATIC ssdp.h ssdp.c)

# set output directories
set_target_properties(ssdp-connect PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
