cmake_minimum_required(VERSION 3.15)
project(I_quantum_algorithms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(I_quantum_algorithms_main src/main.cpp)
    target_include_directories(I_quantum_algorithms_main PRIVATE src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp")
    add_executable(I_quantum_algorithms_test tests/test_main.cpp)
    target_include_directories(I_quantum_algorithms_test PRIVATE src)
endif()
