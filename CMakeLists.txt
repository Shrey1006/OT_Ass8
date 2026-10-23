cmake_minimum_required(VERSION 3.15)
project(Assignment8_OptimizeAnything LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

# Build all track subdirectories
add_subdirectory(A_continuous)
add_subdirectory(B_discrete)
add_subdirectory(C_quantum)
add_subdirectory(D_submodular_parallel)
add_subdirectory(E_case_study)
add_subdirectory(F_compiler)
add_subdirectory(G_xv6)
add_subdirectory(H_dbms)
add_subdirectory(I_quantum_algorithms)
add_subdirectory(J_softeng)
