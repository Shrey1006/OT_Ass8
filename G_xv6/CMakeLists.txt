cmake_minimum_required(VERSION 3.15)
project(G_xv6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(G_xv6_main src/main.cpp)
    target_include_directories(G_xv6_main PRIVATE src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp")
    add_executable(G_xv6_test tests/test_main.cpp)
    target_include_directories(G_xv6_test PRIVATE src)
endif()
