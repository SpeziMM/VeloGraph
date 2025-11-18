cmake_minimum_required(VERSION 3.10)
project(VeloGraph)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(VeloGraph ${SOURCES})

cmake_minimum_required(VERSION 3.10)
project(VeloGraph)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include_directories(include)

add_executable(VeloGraph ${SOURCES})
