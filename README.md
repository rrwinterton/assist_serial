# AVX-512 Microcode Assist Benchmark (ASSIST.SERIAL)

This project is a micro-benchmark designed to demonstrate the performance penalty incurred on some Intel architectures when mixing legacy SSE instructions with AVX-512 instructions.

## The Problem: Microcode Assists

On certain Intel CPUs (particularly those supporting AVX-512 like Ice Lake, Tiger Lake, and Sapphire Rapids), using a legacy SSE instruction (instructions that do not have the `v` prefix, like `movdqa` or `movq`) while the processor is in an AVX-512 state can trigger a **microcode assist**.

This assist (often identified by the `ASSIST.SERIAL` performance counter) occurs because the hardware must transition between different register state tracking modes. Legacy SSE instructions only operate on the lower 128 bits of the registers and may not be aware of the upper bits used by AVX/AVX-512. The resulting microcode assist causes a significant pipeline stall, serializing execution and drastically reducing performance.

## Benchmark Details

The benchmark compares three versions of an AVX-512 setup block:

1.  **Original Code (Penalty):** Uses legacy SSE instructions (`movdqa`, `movq`) to load data into XMM registers before broadcasting them to ZMM registers. This triggers the `ASSIST.SERIAL` microcode penalty.
2.  **Fixed Code (No Penalty):** Uses the VEX/EVEX encoded equivalents (`vmovdqa`, `vmovq`). This avoids the microcode assist but retains a Read-After-Write (RAW) dependency between the load and the broadcast.
3.  **Optimized Code (Maximum Throughput):** Uses memory-operand broadcasts (`vpbroadcastq` directly from a memory address). This eliminates the intermediate XMM register, avoids the microcode penalty, and breaks the instruction-level dependency chain for better pipeline utilization.

### Implementation

The core logic is implemented using inline assembly in `main.cpp`. It measures the time taken to execute the specified number of iterations (default 10,000,000) for each block.

### Example Comparisons

**Original (Slow):**
```asm
movdqa     (%[yuvconstants]),%%xmm8    ; Legacy SSE load (Triggers Assist)
vpbroadcastq %%xmm8, %%zmm8            ; AVX-512 broadcast
```

**Fixed (Fast):**
```asm
vmovdqa    (%[yuvconstants]),%%xmm8    ; VEX encoded load (No Assist)
vpbroadcastq %%xmm8, %%zmm8            ; AVX-512 broadcast (RAW Dependency)
```

**Optimized (Fastest):**
```asm
vpbroadcastq (%[yuvconstants]), %%zmm8 ; Single instruction memory broadcast
```

## Prerequisites

-   A CPU with AVX-512 support (Foundation and BW sets).
-   A compiler that supports AVX-512 inline assembly and target attributes (e.g., Clang, GCC).
-   CMake 3.10 or higher.

## Runtime Hardware Check

The application includes a built-in runtime check using `CPUID` and `XGETBV` to verify that both the CPU and the Operating System support the required AVX-512 Foundation (F) and Byte/Word (BW) extensions.

If the required features are missing, the program will display a detailed error message and exit gracefully instead of crashing with an illegal instruction exception.

## Building and Running

### Windows (Recommended)
Convenience scripts are provided for building with Clang and Ninja on Windows:

*   **Release with Symbols:** Run `build.cmd`. This creates a `build` directory and compiles the project in `RelWithDebInfo` mode.
*   **Debug:** Run `build_debug.cmd`. This creates a `build_debug` directory and compiles the project in `Debug` mode.

### Manual Build
```bash
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja
./avx_benchmark [OPTIONS]
```

### Command Line Options

The benchmark supports the following options:

*   `-h,--help`: Display the help message and exit.
*   `-v,--version`: Display the version information and exit.
*   `-i,--iterations INT`: Number of iterations to run (default: 10,000,000). Must be a positive number.
*   `-t,--test {original,fixed,optimized,all}`: Specify which test to run. 
    *   `original`: Run only the code with legacy SSE instructions.
    *   `fixed`: Run only the code with VEX/EVEX encoded instructions.
    *   `optimized`: Run only the memory-operand broadcast code.
    *   `all` (default): Run all tests and compare performance.

**Examples:**

```bash
# Run all benchmarks with default iterations (10,000,000)
./avx_benchmark

# Run a specific test with custom iterations
./avx_benchmark --iterations 5000000 --test fixed

# Run the optimized test only
./avx_benchmark -t optimized
```

The benchmark uses `-O3 -mavx512f -mavx512bw` flags to ensure the compiler generates the correct instructions and minimizes loop overhead.

## Results

When run on a vulnerable CPU, the benchmark provides several comparisons:

1.  **Fixed vs Original:** The "Fixed" code (using VEX `vmovq`) is typically **2x to 5x faster** than the "Original" code, demonstrating the massive overhead of the serializing microcode assists.
2.  **Optimized vs Fixed:** The "Optimized" code (using memory-operand broadcasts) provides an additional speedup by reducing port pressure and breaking instruction-level dependency chains.
3.  **Optimized vs Original:** The cumulative speedup of the optimized version over the original legacy SSE implementation can often exceed **8x to 10x**.
