cmake_minimum_required(VERSION 3.10)
project(VeloGraph)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(VeloGraph ${SOURCES})

# Link libraries for OSM PBF parsing
target_link_libraries(VeloGraph 
    Threads::Threads
    z
    bz2
    expat
)

# Compiler flags for optimization
target_compile_options(VeloGraph PRIVATE -O3 -march=native)
