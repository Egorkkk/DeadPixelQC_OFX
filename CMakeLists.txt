cmake_minimum_required(VERSION 3.15)
project(DeadPixelQC_OFX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Configuration options
option(BUILD_PLUGIN "Build OFX plugin" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_TOOLS "Build tools" ON)
option(DPQC_MINIMAL_UI "Build minimal UI-only plugin" ON)
option(DPQC_ENABLE_SPATIAL "Enable spatial worker path" OFF)
option(DPQC_ENABLE_TEMPORAL "Enable temporal worker path" OFF)
option(DPQC_ENABLE_FIX "Enable fix worker path" OFF)

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
    set(DEADPIXELQC_PLUGIN_SOURCES
        plugin/PluginMain.cpp
        plugin/Workers/SpatialWorker.cpp
        plugin/Workers/TemporalWorker.cpp
        plugin/Workers/FixWorker.cpp
    )

    if(NOT DPQC_MINIMAL_UI)
        list(APPEND DEADPIXELQC_PLUGIN_SOURCES
            plugin/PluginParams.cpp
            plugin/PluginParamsConstants.cpp
            plugin/PluginParamsBasic.cpp
            plugin/PluginParamsDetection.cpp
            plugin/PluginParamsTemporal.cpp
            plugin/PluginParamsRepair.cpp
            plugin/PluginParamsWorkflow.cpp
            plugin/PluginParamsGetters.cpp
            plugin/OfxUtil.cpp
            model/EventModel.cpp
            log/EventLogger.cpp
            model/DefectMap.cpp
            log/MapAccumulator.cpp
            export/ExportCSV.cpp
            export/ExportJSON.cpp
            export/ExportDefectMap.cpp
            export/ImportDefectMap.cpp
        )
    endif()

    add_library(DeadPixelQC_OFX MODULE ${DEADPIXELQC_PLUGIN_SOURCES})
    
    target_link_libraries(DeadPixelQC_OFX DeadPixelQC_core)
    target_compile_definitions(DeadPixelQC_OFX PRIVATE
        DPQC_MINIMAL_UI=$<IF:$<BOOL:${DPQC_MINIMAL_UI}>,1,0>
        DPQC_ENABLE_SPATIAL=$<IF:$<BOOL:${DPQC_ENABLE_SPATIAL}>,1,0>
        DPQC_ENABLE_TEMPORAL=$<IF:$<BOOL:${DPQC_ENABLE_TEMPORAL}>,1,0>
        DPQC_ENABLE_FIX=$<IF:$<BOOL:${DPQC_ENABLE_FIX}>,1,0>
    )
    
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
