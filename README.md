# AVX-512 Microcode Assist Benchmark (ASSIST.SERIAL)

This project is a micro-benchmark designed to demonstrate the performance penalty incurred on some Intel architectures when mixing legacy SSE instructions with AVX-512 instructions.

## The Problem: Microcode Assists

On certain Intel CPUs (particularly those supporting AVX-512 like Ice Lake, Tiger Lake, and Sapphire Rapids), using a legacy SSE instruction (instructions that do not have the `v` prefix, like `movdqa` or `movq`) while the processor is in an AVX-512 state can trigger a **microcode assist**.

This assist (often identified by the `ASSIST.SERIAL` performance counter) occurs because the hardware must transition between different register state tracking modes. Legacy SSE instructions only operate on the lower 128 bits of the registers and may not be aware of the upper bits used by AVX/AVX-512. The resulting microcode assist causes a significant pipeline stall, serializing execution and drastically reducing performance.

## Benchmark Details

The benchmark compares two versions of an AVX-512 setup block:

1.  **Original Code (Penalty):** Uses legacy SSE instructions (`movdqa`, `movq`) to load data into XMM registers before broadcasting them to ZMM registers.
2.  **Fixed Code (No Penalty):** Uses the VEX/EVEX encoded equivalents (`vmovdqa`, `vmovq`).

### Implementation

The core logic is implemented using inline assembly in `main.cpp`. It measures the time taken to execute 10,000,000 iterations of each block.

### Example Penalty

In the "Original" code:
```asm
movdqa     (%[yuvconstants]),%%xmm8    ; Legacy SSE load
vpbroadcastq %%xmm8, %%zmm8            ; AVX-512 broadcast
```
The `movdqa` triggers an assist because it's interleaved with AVX-512 instructions that utilize the full `zmm` width.

In the "Fixed" code:
```asm
vmovdqa    (%[yuvconstants]),%%xmm8    ; VEX encoded load
vpbroadcastq %%xmm8, %%zmm8            ; AVX-512 broadcast
```
The `vmovdqa` is VEX-encoded and compatible with the AVX-512 state, avoiding the penalty.

## Prerequisites

-   A CPU with AVX-512 support (Foundation and BW sets).
-   A compiler that supports AVX-512 inline assembly (e.g., Clang, GCC).
-   CMake 3.10 or higher.

## Building and Running

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
./avx_benchmark [OPTIONS]
```

### Command Line Options

The benchmark supports the following options:

*   `-h,--help`: Display the help message and exit.
*   `-t,--test {original,fixed,both}`: Specify which test to run. 
    *   `original`: Run only the code with legacy SSE instructions.
    *   `fixed`: Run only the code with VEX/EVEX encoded instructions.
    *   `both` (default): Run both and compare performance.

Example:
```bash
./avx_benchmark --test fixed
```

The benchmark uses `-O3 -mavx512f -mavx512bw` flags to ensure the compiler generates the correct instructions and minimizes loop overhead.

## Results

When run on a vulnerable CPU, the "Fixed" code is typically **2x to 5x faster** than the "Original" code, clearly demonstrating the overhead of the serializing microcode assists.
