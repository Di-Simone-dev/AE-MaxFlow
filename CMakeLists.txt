cmake_minimum_required(VERSION 3.16)
project(MaxFlow CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/util
    ${CMAKE_SOURCE_DIR}/src/push_relabel
    ${CMAKE_SOURCE_DIR}/src/almost_linear
    ${CMAKE_SOURCE_DIR}/src/capacity_scaling
    ${CMAKE_SOURCE_DIR}/third_party/eigen-5.0.0
)

# Main executable
add_executable(maxflow
    main.cpp
    src/push_relabel/push_relabel.cpp
    src/almost_linear/almost_linear_time.cpp
    src/almost_linear/feasible_flow.cpp
    src/almost_linear/min_cost_flow_instance.cpp
)

# Compiler flags per Visual Studio
if(MSVC)
    target_compile_options(maxflow PRIVATE /W4 /EHsc)
else()
    target_compile_options(maxflow PRIVATE -Wall -Wextra -Wpedantic)
endif()
