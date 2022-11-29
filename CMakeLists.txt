cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

project(uvcomms)

#include(${CMAKE_BINARY_DIR}/conan_paths.cmake OPTIONAL)

find_package(uvw CONFIG REQUIRED)

add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(commlib)