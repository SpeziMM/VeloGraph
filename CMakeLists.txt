cmake_minimum_required(VERSION 3.10)
project(VeloGraph)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_TOOLS "Build standalone tools in tools/ (parse_only, inspect_graph, ...)" ON)

# Include directories
include_directories(include)

# macOS Homebrew paths
if(APPLE)
    include_directories(/opt/homebrew/include)
    include_directories(/usr/local/include)
    link_directories(/opt/homebrew/lib)
    link_directories(/usr/local/lib)
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(EXPAT REQUIRED)

# Shared engine sources (everything in src/ except the CLI entry point) compiled
# once into an object library, reused by the main binary and every tool.
file(GLOB CORE_SOURCES "src/*.cpp")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(velograph_core OBJECT ${CORE_SOURCES})
target_compile_options(velograph_core PRIVATE -O3 -march=native)

set(VELOGRAPH_LIBS
    Threads::Threads
    ${ZLIB_LIBRARIES}
    ${BZIP2_LIBRARIES}
    ${EXPAT_LIBRARIES}
)

# Main executable
add_executable(VeloGraph src/main.cpp $<TARGET_OBJECTS:velograph_core>)
target_compile_options(VeloGraph PRIVATE -O3 -march=native)
target_link_libraries(VeloGraph ${VELOGRAPH_LIBS})

# Standalone tools — each links the shared core objects so src/ isn't recompiled per tool.
if(BUILD_TOOLS)
    foreach(tool parse_only parse_build_only inspect_graph get_first_node)
        add_executable(${tool} tools/${tool}.cpp $<TARGET_OBJECTS:velograph_core>)
        target_compile_options(${tool} PRIVATE -O3 -march=native)
        target_link_libraries(${tool} ${VELOGRAPH_LIBS})
    endforeach()
endif()
