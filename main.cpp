#include <iostream>
#include <chrono>
#include <cstdint>
#include <iomanip>

// Mock data structures aligned to 64 bytes (AVX-512 ZMM width)
alignas(64) uint8_t yuvconstants[256] = {0};
alignas(64) uint8_t quadsplitperm[64] = {0};
alignas(64) uint8_t dquadsplitperm[64] = {0};
alignas(64) uint8_t unperm[64] = {0};

// MACRO 1: The Original code (Triggers microcode assist penalty)
#define YUVTORGB_SETUP_AVX512BW_ORIGINAL(p_yuv, p_qsp, p_dqsp, p_unp) \
    __asm__ volatile ( \
        "vpcmpeqb   %%xmm13,%%xmm13,%%xmm13                             \n" \
        "movdqa     (%[yuvconstants]),%%xmm8                            \n" \
        "vpbroadcastq %%xmm8, %%zmm8                                    \n" \
        "vpsllw     $7,%%xmm13,%%xmm13                                  \n" \
        "vpbroadcastb %%xmm13,%%zmm13                                   \n" \
        "movq       32(%[yuvconstants]),%%xmm9                          \n" \
        "vpbroadcastq %%xmm9,%%zmm9                                     \n" \
        "movq       64(%[yuvconstants]),%%xmm10                         \n" \
        "vpbroadcastq %%xmm10,%%zmm10                                   \n" \
        "movq       96(%[yuvconstants]),%%xmm11                         \n" \
        "vpbroadcastq %%xmm11,%%zmm11                                   \n" \
        "movq       128(%[yuvconstants]),%%xmm12                        \n" \
        "vpbroadcastq %%xmm12,%%zmm12                                   \n" \
        "vmovups    (%[quadsplitperm]),%%zmm16                          \n" \
        "vmovups    (%[dquadsplitperm]),%%zmm17                         \n" \
        "vmovups    (%[unperm]),%%zmm18                                 \n" \
        : /* No outputs */ \
        : [yuvconstants] "r" (p_yuv), \
          [quadsplitperm] "r" (p_qsp), \
          [dquadsplitperm] "r" (p_dqsp), \
          [unperm] "r" (p_unp) \
        : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", \
          "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", \
          "zmm16", "zmm17", "zmm18", "memory" \
    )

// MACRO 2: The Fixed code (VEX/EVEX encoded only)
#define YUVTORGB_SETUP_AVX512BW_FIXED(p_yuv, p_qsp, p_dqsp, p_unp) \
    __asm__ volatile ( \
        "vpcmpeqb   %%xmm13,%%xmm13,%%xmm13                             \n" \
        "vmovdqa    (%[yuvconstants]),%%xmm8                            \n" \
        "vpbroadcastq %%xmm8, %%zmm8                                    \n" \
        "vpsllw     $7,%%xmm13,%%xmm13                                  \n" \
        "vpbroadcastb %%xmm13,%%zmm13                                   \n" \
        "vmovq      32(%[yuvconstants]),%%xmm9                          \n" \
        "vpbroadcastq %%xmm9,%%zmm9                                     \n" \
        "vmovq      64(%[yuvconstants]),%%xmm10                         \n" \
        "vpbroadcastq %%xmm10,%%zmm10                                   \n" \
        "vmovq      96(%[yuvconstants]),%%xmm11                         \n" \
        "vpbroadcastq %%xmm11,%%zmm11                                   \n" \
        "vmovq      128(%[yuvconstants]),%%xmm12                        \n" \
        "vpbroadcastq %%xmm12,%%zmm12                                   \n" \
        "vmovups    (%[quadsplitperm]),%%zmm16                          \n" \
        "vmovups    (%[dquadsplitperm]),%%zmm17                         \n" \
        "vmovups    (%[unperm]),%%zmm18                                 \n" \
        : /* No outputs */ \
        : [yuvconstants] "r" (p_yuv), \
          [quadsplitperm] "r" (p_qsp), \
          [dquadsplitperm] "r" (p_dqsp), \
          [unperm] "r" (p_unp) \
        : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", \
          "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", \
          "zmm16", "zmm17", "zmm18", "memory" \
    )

int main() {
    const int iterations = 10000000; // 10 Million iterations
    
    // Pointers to pass into the asm blocks
    uint8_t* p_yuv = yuvconstants;
    uint8_t* p_qsp = quadsplitperm;
    uint8_t* p_dqsp = dquadsplitperm;
    uint8_t* p_unp = unperm;

    std::cout << "Starting AVX-512 Microcode Assist Benchmark...\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    // --- TEST 1: ORIGINAL CODE (Penalty) ---
    auto start_orig = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        YUVTORGB_SETUP_AVX512BW_ORIGINAL(p_yuv, p_qsp, p_dqsp, p_unp);
    }
    auto end_orig = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_orig = end_orig - start_orig;

    // --- TEST 2: FIXED CODE (No Penalty) ---
    auto start_fixed = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        YUVTORGB_SETUP_AVX512BW_FIXED(p_yuv, p_qsp, p_dqsp, p_unp);
    }
    auto end_fixed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_fixed = end_fixed - start_fixed;

    // --- RESULTS ---
    std::cout << "Results:\n";
    std::cout << "------------------------------------------\n";
    std::cout << "Original Code Time: " << std::fixed << std::setprecision(2) << duration_orig.count() << " ms\n";
    std::cout << "Fixed Code Time:    " << std::fixed << std::setprecision(2) << duration_fixed.count() << " ms\n";
    std::cout << "------------------------------------------\n";
    
    if (duration_fixed.count() < duration_orig.count()) {
        double speedup = duration_orig.count() / duration_fixed.count();
        std::cout << "Speedup: " << speedup << "x faster with fixed AVX-512 instructions.\n";
    }

    return 0;
}
