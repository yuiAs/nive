# plugins/CMakeLists.txt
# Independent CMake project for nive plugins

cmake_minimum_required(VERSION 3.28)
project(nive-plugins LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate nive plugin API header
set(NIVE_PLUGIN_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    CACHE PATH "Path to nive plugin API headers")

add_subdirectory(avif)
