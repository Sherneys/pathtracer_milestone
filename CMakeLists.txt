cmake_minimum_required(VERSION 3.20)
project(RayTracingMilestone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for release builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Milestone CPU path tracer (self-contained)
add_executable(pathtracer main.cpp)
