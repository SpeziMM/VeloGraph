cmake_minimum_required(VERSION 3.10)
project(VeloGraph)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

# Source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(VeloGraph ${SOURCES})

# Link libraries for OSM PBF parsing
target_link_libraries(VeloGraph 
    Threads::Threads
    ${ZLIB_LIBRARIES}
    ${BZIP2_LIBRARIES}
    ${EXPAT_LIBRARIES}
)

# Compiler flags for optimization
target_compile_options(VeloGraph PRIVATE -O3 -march=native)
