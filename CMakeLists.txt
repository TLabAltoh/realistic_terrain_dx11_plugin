cmake_minimum_required(VERSION 3.4...3.18)

project(dx11_erosion)

set(PYTHON_EXECUTABLE "${PROJECT_SOURCE_DIR}/Python-3.10.8/PCbuild/amd64/python.exe")
set(PYTHON_LIBRARY "${PROJECT_SOURCE_DIR}/Python-3.10.8/PCbuild/amd64/python310.lib")
set(PYTHON_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/Python-3.10.8/Include")

add_subdirectory(pybind11)

pybind11_add_module(dx11_erosion src/main.cpp)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(dx11_erosion
                           PRIVATE VERSION_INFO = 1.0.0)

## TEST CMAKE
# cmake_minimum_required(VERSION 3.4...3.18)
# project(direct_compute_erosion)

# add_subdirectory(pybind11)
# set(SOURCES src/main.cpp)

# pybind11_add_module(direct_compute_erosion ${SOURCES})
