cmake_minimum_required(VERSION 3.15)
project(DeadPixelQC_OFX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configuration options
option(BUILD_PLUGIN "Build OFX plugin" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_TOOLS "Build tools" ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/ofx-sdk/include
)

# Core library (header-only implementation)
add_library(DeadPixelQC_core INTERFACE)

target_include_directories(DeadPixelQC_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# Plugin
if(BUILD_PLUGIN)
    add_library(DeadPixelQC_OFX MODULE
        plugin/PluginMain.cpp
        plugin/PluginParams.cpp
        plugin/PluginParamsConstants.cpp
        plugin/PluginParamsBasic.cpp
        plugin/PluginParamsDetection.cpp
        plugin/PluginParamsTemporal.cpp
        plugin/PluginParamsRepair.cpp
        plugin/PluginParamsGetters.cpp
        plugin/OfxUtil.cpp
    )
    
    target_link_libraries(DeadPixelQC_OFX DeadPixelQC_core)
    
    # Set plugin properties
    if(WIN32)
        set_target_properties(DeadPixelQC_OFX PROPERTIES
            SUFFIX ".ofx"
            PREFIX ""
        )
    elseif(APPLE)
        set_target_properties(DeadPixelQC_OFX PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "ofx"
        )
    else()
        set_target_properties(DeadPixelQC_OFX PROPERTIES
            SUFFIX ".ofx"
            PREFIX ""
        )
    endif()
endif()

# Tests
if(BUILD_TESTS)
    add_executable(DeadPixelQC_tests
        tests/UnitTests.cpp
        tests/SyntheticGenerator.cpp
    )
    
    target_link_libraries(DeadPixelQC_tests DeadPixelQC_core)
    
    # Add test data generation
    add_custom_command(
        TARGET DeadPixelQC_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Tests built successfully"
    )
endif()

# Tools
if(BUILD_TOOLS)
    add_executable(offline_runner
        tools/offline_runner.cpp
    )
    
    target_link_libraries(offline_runner DeadPixelQC_core)
    
    # Find image libraries
    find_package(OpenCV QUIET)
    if(OpenCV_FOUND)
        target_link_libraries(offline_runner ${OpenCV_LIBS})
        target_include_directories(offline_runner PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_compile_definitions(offline_runner PRIVATE HAS_OPENCV)
    else()
        message(WARNING "OpenCV not found. offline_runner will have limited functionality.")
    endif()
endif()

# Documentation
add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E echo "Generating documentation..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_BINARY_DIR}/docs
)

# Installation
if(BUILD_PLUGIN)
    if(WIN32)
        install(TARGETS DeadPixelQC_OFX
            DESTINATION "C:/ProgramData/Blackmagic Design/DaVinci Resolve/Support/OFX/Plugins"
        )
    elseif(APPLE)
        install(TARGETS DeadPixelQC_OFX
            BUNDLE DESTINATION "~/Library/OFX/Plugins"
        )
    else()
        install(TARGETS DeadPixelQC_OFX
            DESTINATION "/usr/OFX/Plugins"
        )
    endif()
endif()
