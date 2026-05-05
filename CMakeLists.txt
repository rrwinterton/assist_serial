cmake_minimum_required(VERSION 3.10)
project(AVX512_Penalty_Benchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Clang to support AVX-512 Foundation and Byte/Word instructions
# -O3 ensures loop overhead is minimized so we strictly measure the asm blocks
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mavx512f -mavx512bw")

add_executable(avx_benchmark main.cpp)
