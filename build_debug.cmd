@echo off
if exist build_debug rmdir /s /q build_debug
mkdir build_debug
cd build_debug

:: Generate the Ninja build files using Clang with Debug configuration
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug ..

:: Compile the code
ninja
