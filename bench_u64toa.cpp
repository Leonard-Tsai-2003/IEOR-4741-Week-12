// bench_u64toa.cpp — HW12 Benchmark & Correctness Harness
// Measures cycles/op (and ns/op) across magnitudes comparing:
//   1. u64toa_baseline (before: single-digit /10 + %10 + reverse copy)
//   2. u64toa          (after:  CLZ exact length + 8/4/2-digit table batching)
//   3. std::to_string  (C++ standard library, heap-allocates past 15-char SSO)
//   4. std::snprintf   (C stdio formatter, format parsing + locale overhead)
// bench_u64toa.cpp — HW12 Benchmark & Correctness Harness

// g++ -O3 -std=c++17 -march=native -Wall -Wextra -Iinclude bench_u64toa.cpp u64toa.cpp -o /tmp/bench_u64toa && /tmp/bench_u64toa

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include "u64toa.hpp"

namespace {

template <typename T>
inline void do_not_optimize(const T& value) noexcept {
    asm volatile("" : : "r,m"(value) : "memory");
}

void pin_to_single_core() noexcept {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
#endif
}

inline std::uint64_t read_tsc_start() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
    return __rdtsc();
#elif defined(__aarch64__)
    std::uint64_t val;
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

inline std::uint64_t read_tsc_end() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned aux = 0;
    const std::uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
#elif defined(__aarch64__)
    std::uint64_t val;
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

double calibrate_tsc_ghz() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    for (int attempt = 0; attempt < 5; ++attempt) {
        const auto w0 = std::chrono::steady_clock::now();
        const std::uint64_t c0 = read_tsc_start();

        std::uint64_t spin = 0;
        for (int i = 0; i < 2000000; ++i) {
            spin += static_cast<std::uint64_t>(i);
            do_not_optimize(spin);
        }

        const std::uint64_t c1 = read_tsc_end();
        const auto w1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(w1 - w0).count();

        if (c1 > c0 && ns > 10000.0) {
            const double ghz = static_cast<double>(c1 - c0) / ns;
            if (ghz >= 0.01 && ghz <= 10.0) {
                return ghz;
            }
        }
    }
#endif
    return 0.0;
}

struct BenchStats {
    double cycles_per_op;
    double ns_per_op;
};

template <typename Fn>
BenchStats benchmark_fn(Fn&& fn, int total_iters, double tsc_ghz) {
    constexpr int kBatchSize = 10000;
    const int batches = std::max(20, total_iters / kBatchSize);

    // Warmup
    for (int i = 0; i < kBatchSize * 2; ++i) {
        fn();
    }

    std::vector<double> ns_samples;
    std::vector<double> cyc_samples;
    ns_samples.reserve(batches);
    cyc_samples.reserve(batches);

    for (int b = 0; b < batches; ++b) {
        const auto wall0 = std::chrono::steady_clock::now();
        const std::uint64_t c0 = read_tsc_start();

        for (int i = 0; i < kBatchSize; ++i) {
            fn();
        }

        const std::uint64_t c1 = read_tsc_end();
        const auto wall1 = std::chrono::steady_clock::now();

        const double ns = std::chrono::duration<double, std::nano>(wall1 - wall0).count() /
                          static_cast<double>(kBatchSize);
        if (ns > 0.0) {
            ns_samples.push_back(ns);
        }

        // Reject unsigned wraparound (c1 <= c0) and cross-core TSC spikes
        if (tsc_ghz > 0.0 && c1 > c0) {
            const double cyc = static_cast<double>(c1 - c0) / static_cast<double>(kBatchSize);
            if (ns <= 0.0 || cyc < ns * tsc_ghz * 10.0) {
                cyc_samples.push_back(cyc);
            }
        }
    }

    std::sort(ns_samples.begin(), ns_samples.end());
    const double best_ns = ns_samples[ns_samples.size() / 10]; // 10th percentile

    double best_cycles = 0.0;
    if (!cyc_samples.empty()) {
        std::sort(cyc_samples.begin(), cyc_samples.end());
        best_cycles = cyc_samples[cyc_samples.size() / 10];
    } else {
        best_cycles = best_ns * 3.0; // Fallback if TSC is virtualized/non-monotonic
    }

    return {best_cycles, best_ns};
}

bool verify_single(std::uint64_t v) {
    char buf_opt[32] = {};
    char buf_base[32] = {};
    const int n_opt = u64toa(v, buf_opt);
    const int n_out = u64toa_outlined(v, buf_base);
    buf_opt[n_opt] = '\0';
    buf_base[n_out] = '\0';

    const std::string expected = std::to_string(v);
    if (static_cast<int>(expected.size()) != n_opt || expected != buf_opt) {
        std::cerr << "[FAIL] u64toa(" << v << ") -> \"" << buf_opt
                  << "\" (len=" << n_opt << "), expected \"" << expected << "\"\n";
        return false;
    }
    if (n_opt != n_out || std::strcmp(buf_opt, buf_base) != 0) {
        std::cerr << "[FAIL] u64toa_outlined mismatch on " << v << "\n";
        return false;
    }
    return true;
}

bool run_correctness_suite() {
    std::vector<std::uint64_t> edge_cases = {
        0ULL, 1ULL, 2ULL, 3ULL, 4ULL, 5ULL, 6ULL, 7ULL, 8ULL, 9ULL,
        10ULL, 11ULL, 42ULL, 99ULL, 100ULL, 101ULL, 999ULL, 1000ULL, 9999ULL, 10000ULL,
        12345ULL, 99999ULL, 100000ULL, 1000000ULL,
        0x7FFFFFFFULL, 0x80000000ULL, 0xFFFFFFFFULL, 0x100000000ULL,
        9999999999ULL, 123456789012345ULL,
        UINT64_MAX - 1ULL, UINT64_MAX
    };

    std::uint64_t p10 = 1;
    for (int d = 1; d <= 20; ++d) {
        edge_cases.push_back(p10 - 1);
        edge_cases.push_back(p10);
        edge_cases.push_back(p10 + 1);
        if (d < 20) p10 *= 10ULL;
    }

    for (std::uint64_t v : edge_cases) {
        if (!verify_single(v)) return false;
    }

    std::uint64_t rng = 88172645463325252ULL;
    for (int i = 0; i < 500000; ++i) {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        const std::uint64_t masked = rng >> (i & 63);
        if (!verify_single(masked)) return false;
    }
    return true;
}

} // namespace

int main() {
    pin_to_single_core();

    if (!run_correctness_suite()) {
        return 1;
    }
    std::cout << "Correctness check PASSED (all edge cases + 500k fuzz values).\n";

    const double tsc_ghz = calibrate_tsc_ghz();
    if (tsc_ghz > 0.0) {
        std::printf("Timer: hardware TSC calibrated at %.2f GHz (invariant/monotonic).\n\n", tsc_ghz);
    } else {
        std::printf("Timer: non-monotonic/virtualized TSC detected; deriving cycles from steady_clock @ 3.0 GHz.\n\n");
    }

    struct MagnitudeCase {
        const char* label;
        std::uint64_t value;
    };

    constexpr std::array<MagnitudeCase, 8> kCases = {{
        {"0 (1 digit)",                      0ULL},
        {"7 (1 digit)",                      7ULL},
        {"42 (2 digits)",                    42ULL},
        {"12,345 (5 digits)",                12345ULL},
        {"1,000,000,000 (10 digits)",        1000000000ULL},
        {"123,456,789,012,345 (15 digits)",  123456789012345ULL},
        {"1,000,000,000,000,000,000 (19d)",  1000000000000000000ULL},
        {"UINT64_MAX (20 digits)",           UINT64_MAX},
    }};

    char buf[32];

    std::printf("%-32s | %9s | %9s | %9s | %9s | %8s | %8s | %8s\n",
                "Magnitude", "Before", "After", "to_string", "snprintf",
                "vsBefore", "vsString", "vsSnprtf");
    std::printf("---------------------------------+-----------+-----------+-----------+-----------+----------+----------+----------\n");

    for (const auto& tc : kCases) {
        std::uint64_t v = tc.value;

        const BenchStats st_before = benchmark_fn([&] {
            do_not_optimize(v);
            int n = u64toa_baseline(v, buf);
            do_not_optimize(n);
            do_not_optimize(buf[0]);
        }, 4000000, tsc_ghz);

        const BenchStats st_after = benchmark_fn([&] {
            do_not_optimize(v);
            int n = u64toa(v, buf);
            do_not_optimize(n);
            do_not_optimize(buf[0]);
        }, 4000000, tsc_ghz);

        const BenchStats st_tostr = benchmark_fn([&] {
            do_not_optimize(v);
            std::string s = std::to_string(v);
            do_not_optimize(s.data());
            do_not_optimize(s[0]);
        }, 1000000, tsc_ghz);

        const BenchStats st_snpr = benchmark_fn([&] {
            do_not_optimize(v);
            int n = std::snprintf(buf, sizeof(buf), "%" PRIu64, v);
            do_not_optimize(n);
            do_not_optimize(buf[0]);
        }, 1000000, tsc_ghz);

        std::printf("%-32s | %6.2f cy | %6.2f cy | %6.2f cy | %6.2f cy | %7.2fx | %7.2fx | %7.2fx\n",
                    tc.label,
                    st_before.cycles_per_op,
                    st_after.cycles_per_op,
                    st_tostr.cycles_per_op,
                    st_snpr.cycles_per_op,
                    st_before.cycles_per_op / st_after.cycles_per_op,
                    st_tostr.cycles_per_op / st_after.cycles_per_op,
                    st_snpr.cycles_per_op / st_after.cycles_per_op);
    }

    std::array<std::uint64_t, 1024> mixed{};
    std::uint64_t rng = 0x123456789ABCDEF0ULL;
    std::uint64_t pow10_table[20];
    pow10_table[0] = 1ULL;
    for (int i = 1; i < 20; ++i) pow10_table[i] = pow10_table[i - 1] * 10ULL;

    for (std::size_t i = 0; i < mixed.size(); ++i) {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        const int digits = static_cast<int>(i % 20);
        const std::uint64_t lo = (digits == 0) ? 0ULL : pow10_table[digits];
        const std::uint64_t hi = (digits == 19) ? UINT64_MAX : (pow10_table[digits + 1] - 1ULL);
        mixed[i] = lo + (rng % (hi - lo + 1ULL));
    }

    std::size_t idx = 0;
    const BenchStats mix_before = benchmark_fn([&] {
        const std::uint64_t v = mixed[idx++ & 1023U];
        int n = u64toa_baseline(v, buf);
        do_not_optimize(n);
        do_not_optimize(buf[0]);
    }, 4000000, tsc_ghz);

    idx = 0;
    const BenchStats mix_after = benchmark_fn([&] {
        const std::uint64_t v = mixed[idx++ & 1023U];
        int n = u64toa(v, buf);
        do_not_optimize(n);
        do_not_optimize(buf[0]);
    }, 4000000, tsc_ghz);

    idx = 0;
    const BenchStats mix_tostr = benchmark_fn([&] {
        const std::uint64_t v = mixed[idx++ & 1023U];
        std::string s = std::to_string(v);
        do_not_optimize(s.data());
        do_not_optimize(s[0]);
    }, 1000000, tsc_ghz);

    idx = 0;
    const BenchStats mix_snpr = benchmark_fn([&] {
        const std::uint64_t v = mixed[idx++ & 1023U];
        int n = std::snprintf(buf, sizeof(buf), "%" PRIu64, v);
        do_not_optimize(n);
        do_not_optimize(buf[0]);
    }, 1000000, tsc_ghz);

    std::printf("---------------------------------+-----------+-----------+-----------+-----------+----------+----------+----------\n");
    std::printf("%-32s | %6.2f cy | %6.2f cy | %6.2f cy | %6.2f cy | %7.2fx | %7.2fx | %7.2fx\n",
                "Mixed (uniform 1..20 digits)",
                mix_before.cycles_per_op,
                mix_after.cycles_per_op,
                mix_tostr.cycles_per_op,
                mix_snpr.cycles_per_op,
                mix_before.cycles_per_op / mix_after.cycles_per_op,
                mix_tostr.cycles_per_op / mix_after.cycles_per_op,
                mix_snpr.cycles_per_op / mix_after.cycles_per_op);

    return 0;
}