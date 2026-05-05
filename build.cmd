@echo off
if exist build rmdir /s /q build
mkdir build
cd build

:: Generate the Ninja build files using Clang with Release + Symbols
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

:: Compile the code
ninja
