cmake_minimum_required(VERSION 3.16)
project(MaxFlow CXX)

find_package(Doxygen REQUIRED)
add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


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
    ${CMAKE_SOURCE_DIR}/Eigen
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Main executable
add_executable(maxflow
    src/main.cpp

    # push-relabel
    src/push_relabel/push_relabel.cpp

    # almost-linear
    src/almost_linear/almost_linear_time.cpp
    src/almost_linear/feasible_flow.cpp
    src/almost_linear/min_cost_flow_instance.cpp
    src/almost_linear/howard.cpp

    # capacity scaling
    src/capacity_scaling/capacity_scaling.cpp

    # parser DIMACS
    src/util/parse_dimacs.cpp

    # rational utilities
    src/util/capacity.cpp
    src/util/fraction.cpp
    src/util/scale_rationals.cpp
)

# Compiler flags per Visual Studio
if(MSVC)
    target_compile_options(maxflow PRIVATE /W4 /EHsc)
else()
    target_compile_options(maxflow PRIVATE -O3  -DNDEBUG)
    #target_compile_options(maxflow PRIVATE -Wall -Wextra -Wpedantic)
endif()
