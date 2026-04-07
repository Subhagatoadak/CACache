cmake_minimum_required(VERSION 3.21)
project(cacache VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release optimizations
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Find Python and nanobind
find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(nanobind CONFIG REQUIRED)

# Core C++ library (static, no Python dep)
add_library(cacache_core STATIC
    src/ca/grid.cpp
    src/ca/rules.cpp
    src/cache/orchestrator.cpp
)

target_include_directories(cacache_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(cacache_core PRIVATE -Wall -Wextra -Wpedantic)

# Python extension module
nanobind_add_module(_cacache_ext
    src/bindings/python.cpp
)

target_link_libraries(_cacache_ext PRIVATE cacache_core)

install(TARGETS _cacache_ext LIBRARY DESTINATION cacache)
