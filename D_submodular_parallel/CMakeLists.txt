cmake_minimum_required(VERSION 3.15)
project(D_submodular_parallel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(D_submodular_parallel_main src/main.cpp)
    target_include_directories(D_submodular_parallel_main PRIVATE src)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(D_submodular_parallel_main PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp")
    add_executable(D_submodular_parallel_test tests/test_main.cpp)
    target_include_directories(D_submodular_parallel_test PRIVATE src)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(D_submodular_parallel_test PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()
