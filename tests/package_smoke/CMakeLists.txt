cmake_minimum_required(VERSION 3.20)

project(lightgraph_package_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(lightgraph CONFIG REQUIRED)

add_executable(lightgraph_package_smoke main.cpp)
target_link_libraries(lightgraph_package_smoke PRIVATE lightgraph::lightgraph)
