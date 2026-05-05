:: Create a build directory and step into it
mkdir build
cd build

:: Generate the Ninja build files using Clang
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ ..

:: Compile the code
ninja

:: Run the benchmark
avx_benchmark.exe
