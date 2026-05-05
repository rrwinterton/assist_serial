#include <iostream>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include <intrin.h>

// Helper to check for AVX-512 Foundation and Byte/Word support
bool check_avx512_support(bool& has_f, bool& has_bw) {
    has_f = false;
    has_bw = false;

    int cpu_info[4];

    // 1. Check maximum leaf supported
    __cpuid(cpu_info, 0);
    if (cpu_info[0] < 7) return false;

    // 2. Check OSXSAVE bit (CPUID.1:ECX bit 27)
    __cpuid(cpu_info, 1);
    bool os_xsave = (cpu_info[2] & (1 << 27)) != 0;
    if (!os_xsave) return false;

    // 3. Check XCR0 for AVX-512 state support (XGETBV)
    unsigned long long xcr0 = _xgetbv(0);
    if ((xcr0 & 0xE0) != 0xE0) return false;

    // 4. Check Feature Bits
    __cpuidex(cpu_info, 7, 0);
    has_f = (cpu_info[1] & (1 << 16)) != 0;
    has_bw = (cpu_info[1] & (1 << 30)) != 0;

    return has_f && has_bw;
}

// Mock data structures aligned to 64 bytes
alignas(64) uint8_t yuvconstants[256] = {0};
alignas(64) uint8_t quadsplitperm[64] = {0};
alignas(64) uint8_t dquadsplitperm[64] = {0};
alignas(64) uint8_t unperm[64] = {0};

// MACRO 1: Original (Triggers microcode assist penalty via legacy SSE)
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
        : : [yuvconstants] "r" (p_yuv), [quadsplitperm] "r" (p_qsp), [dquadsplitperm] "r" (p_dqsp), [unperm] "r" (p_unp) \
        : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm16", "zmm17", "zmm18", "memory" \
    )

// MACRO 2: Fixed (Uses VEX vmovq to avoid penalty, but has RAW dependency)
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
        : : [yuvconstants] "r" (p_yuv), [quadsplitperm] "r" (p_qsp), [dquadsplitperm] "r" (p_dqsp), [unperm] "r" (p_unp) \
        : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm16", "zmm17", "zmm18", "memory" \
    )

// MACRO 3: Optimized (Memory-operand broadcasts, avoids RAW dependency and port pressure)
#define YUVTORGB_SETUP_AVX512BW_OPTIMIZED(p_yuv, p_qsp, p_dqsp, p_unp) \
    __asm__ volatile ( \
        "vpcmpeqb       %%xmm13, %%xmm13, %%xmm13                       \n" \
        "vpbroadcastq   (%[yuvconstants]), %%zmm8                       \n" \
        "vpsllw         $7, %%xmm13, %%xmm13                            \n" \
        "vpbroadcastb   %%xmm13, %%zmm13                                \n" \
        "vpbroadcastq   32(%[yuvconstants]), %%zmm9                     \n" \
        "vpbroadcastq   64(%[yuvconstants]), %%zmm10                    \n" \
        "vpbroadcastq   96(%[yuvconstants]), %%zmm11                    \n" \
        "vpbroadcastq   128(%[yuvconstants]), %%zmm12                   \n" \
        "vmovups        (%[quadsplitperm]), %%zmm16                     \n" \
        "vmovups        (%[dquadsplitperm]), %%zmm17                    \n" \
        "vmovups        (%[unperm]), %%zmm18                            \n" \
        : : [yuvconstants] "r" (p_yuv), [quadsplitperm] "r" (p_qsp), [dquadsplitperm] "r" (p_dqsp), [unperm] "r" (p_unp) \
        : "xmm13", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm16", "zmm17", "zmm18", "memory" \
    )

__attribute__((target("avx512f,avx512bw")))
void run_original_test(int64_t iterations, uint8_t* p_yuv, uint8_t* p_qsp, uint8_t* p_dqsp, uint8_t* p_unp) {
    for (int64_t i = 0; i < iterations; ++i) { YUVTORGB_SETUP_AVX512BW_ORIGINAL(p_yuv, p_qsp, p_dqsp, p_unp); }
}

__attribute__((target("avx512f,avx512bw")))
void run_fixed_test(int64_t iterations, uint8_t* p_yuv, uint8_t* p_qsp, uint8_t* p_dqsp, uint8_t* p_unp) {
    for (int64_t i = 0; i < iterations; ++i) { YUVTORGB_SETUP_AVX512BW_FIXED(p_yuv, p_qsp, p_dqsp, p_unp); }
}

__attribute__((target("avx512f,avx512bw")))
void run_optimized_test(int64_t iterations, uint8_t* p_yuv, uint8_t* p_qsp, uint8_t* p_dqsp, uint8_t* p_unp) {
    for (int64_t i = 0; i < iterations; ++i) { YUVTORGB_SETUP_AVX512BW_OPTIMIZED(p_yuv, p_qsp, p_dqsp, p_unp); }
}

int main(int argc, char** argv) {
    CLI::App app{"AVX-512 Microcode Assist Benchmark"};
    app.set_help_flag("-h,--help", "Print this help message and exit");
    app.set_version_flag("-v,--version", "1.1.0");

    int64_t iterations = 10000000;
    app.add_option("-i,--iterations", iterations, "Number of iterations")
       ->default_val(10000000)->check(CLI::PositiveNumber);

    std::string test_mode = "all";
    app.add_option("-t,--test", test_mode, "Which benchmark test(s) to run")
       ->default_val("all")
       ->check(CLI::IsMember({"original", "fixed", "optimized", "all"}));

    CLI11_PARSE(app, argc, argv);
    
    std::cout << "Checking for AVX-512 support...\n";
    bool has_f, has_bw;
    if (!check_avx512_support(has_f, has_bw)) {
        std::cout << "Error: This system does not support required AVX-512 (F & BW).\n";
        return 1;
    }

    uint8_t *p_yuv = yuvconstants, *p_qsp = quadsplitperm, *p_dqsp = dquadsplitperm, *p_unp = unperm;
    std::cout << "Starting Benchmark (Iterations: " << iterations << ", Mode: " << test_mode << ")\n\n";

    auto run_and_report = [&](const std::string& name, auto func) {
        auto start = std::chrono::high_resolution_clock::now();
        func(iterations, p_yuv, p_qsp, p_dqsp, p_unp);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << std::left << std::setw(15) << name << ": " << std::fixed << std::setprecision(2) << ms << " ms\n";
        return ms;
    };

    double t_orig = 0, t_fixed = 0, t_opt = 0;
    if (test_mode == "original" || test_mode == "all") t_orig = run_and_report("Original", run_original_test);
    if (test_mode == "fixed" || test_mode == "all")    t_fixed = run_and_report("Fixed", run_fixed_test);
    if (test_mode == "optimized" || test_mode == "all") t_opt = run_and_report("Optimized", run_optimized_test);

    if (test_mode == "all") {
        std::cout << "------------------------------------------\n";
        std::cout << "Speedup (Fixed vs Original):     " << (t_orig / t_fixed) << "x\n";
        std::cout << "Speedup (Optimized vs Fixed):    " << (t_fixed / t_opt) << "x\n";
        std::cout << "Speedup (Optimized vs Original): " << (t_orig / t_opt) << "x\n";
    }

    return 0;
}
