// HW12 — fast uint64 -> decimal text. Write the digits into out; return the length.

#pragma once
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// HW12 — Fast uint64 -> decimal ASCII serialization.
// Writes non-null-terminated decimal digits into `out` and returns the character count (1..20).

namespace u64toa_detail {

// 200-byte lookup table mapping 00..99 -> two ASCII decimal digits.
inline constexpr char kDigits100[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};

struct DigitThreshold {
    std::uint64_t pow10;
    int base_digits;
};

// Indexed by bit_index = 63 - clz(v | 1). Each bit_index spans at most two decimal lengths;
// comparing v >= pow10 resolves the exact length in O(1) without branches.
inline constexpr DigitThreshold kDigitThresholds[64] = {
    {10ULL, 1},                    // bit  0: [1, 1]
    {10ULL, 1},                    // bit  1: [2, 3]
    {10ULL, 1},                    // bit  2: [4, 7]
    {10ULL, 1},                    // bit  3: [8, 15]
    {100ULL, 2},                   // bit  4: [16, 31]
    {100ULL, 2},                   // bit  5: [32, 63]
    {100ULL, 2},                   // bit  6: [64, 127]
    {1000ULL, 3},                  // bit  7: [128, 255]
    {1000ULL, 3},                  // bit  8: [256, 511]
    {1000ULL, 3},                  // bit  9: [512, 1023]
    {10000ULL, 4},                 // bit 10
    {10000ULL, 4},                 // bit 11
    {10000ULL, 4},                 // bit 12
    {10000ULL, 4},                 // bit 13
    {100000ULL, 5},                // bit 14
    {100000ULL, 5},                // bit 15
    {100000ULL, 5},                // bit 16
    {1000000ULL, 6},               // bit 17
    {1000000ULL, 6},               // bit 18
    {1000000ULL, 6},               // bit 19
    {10000000ULL, 7},              // bit 20
    {10000000ULL, 7},              // bit 21
    {10000000ULL, 7},              // bit 22
    {10000000ULL, 7},              // bit 23
    {100000000ULL, 8},             // bit 24
    {100000000ULL, 8},             // bit 25
    {100000000ULL, 8},             // bit 26
    {1000000000ULL, 9},            // bit 27
    {1000000000ULL, 9},            // bit 28
    {1000000000ULL, 9},            // bit 29
    {10000000000ULL, 10},          // bit 30
    {10000000000ULL, 10},          // bit 31
    {10000000000ULL, 10},          // bit 32
    {10000000000ULL, 10},          // bit 33
    {100000000000ULL, 11},         // bit 34
    {100000000000ULL, 11},         // bit 35
    {100000000000ULL, 11},         // bit 36
    {1000000000000ULL, 12},        // bit 37
    {1000000000000ULL, 12},        // bit 38
    {1000000000000ULL, 12},        // bit 39
    {10000000000000ULL, 13},       // bit 40
    {10000000000000ULL, 13},       // bit 41
    {10000000000000ULL, 13},       // bit 42
    {10000000000000ULL, 13},       // bit 43
    {100000000000000ULL, 14},      // bit 44
    {100000000000000ULL, 14},      // bit 45
    {100000000000000ULL, 14},      // bit 46
    {1000000000000000ULL, 15},     // bit 47
    {1000000000000000ULL, 15},     // bit 48
    {1000000000000000ULL, 15},     // bit 49
    {10000000000000000ULL, 16},    // bit 50
    {10000000000000000ULL, 16},    // bit 51
    {10000000000000000ULL, 16},    // bit 52
    {10000000000000000ULL, 16},    // bit 53
    {100000000000000000ULL, 17},   // bit 54
    {100000000000000000ULL, 17},   // bit 55
    {100000000000000000ULL, 17},   // bit 56
    {1000000000000000000ULL, 18},  // bit 57
    {1000000000000000000ULL, 18},  // bit 58
    {1000000000000000000ULL, 18},  // bit 59
    {10000000000000000000ULL, 19}, // bit 60
    {10000000000000000000ULL, 19}, // bit 61
    {10000000000000000000ULL, 19}, // bit 62
    {10000000000000000000ULL, 19}  // bit 63
};

inline int exact_decimal_length(std::uint64_t v) noexcept {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse64(&idx, v | 1ULL);
    const int bit = static_cast<int>(idx);
#else
    const int bit = 63 - __builtin_clzll(v | 1ULL);
#endif
    const DigitThreshold entry = kDigitThresholds[bit];
    return entry.base_digits + static_cast<int>(v >= entry.pow10);
}

inline void write_4digits(char* dst, std::uint32_t r) noexcept {
    const std::uint32_t hi = r / 100U;
    const std::uint32_t lo = r - hi * 100U;
    std::memcpy(dst,     &kDigits100[hi * 2U], 2);
    std::memcpy(dst + 2, &kDigits100[lo * 2U], 2);
}

inline void write_8digits(char* dst, std::uint32_t r) noexcept {
    const std::uint32_t hi4 = r / 10000U;
    const std::uint32_t lo4 = r - hi4 * 10000U;
    write_4digits(dst,     hi4);
    write_4digits(dst + 4, lo4);
}

} // namespace u64toa_detail

// Optimized implementation:
// 1. Branchless O(1) exact decimal length via CLZ + threshold table -> writes directly into `out`
//    with zero temporary buffer and zero second reverse-copy loop.
// 2. Hierarchical 8/4/2-digit batching with a 200-byte 2-digit lookup table (`kDigits100`),
//    cutting divisions by up to 8x and enabling instruction-level parallelism in 32-bit registers.
inline int u64toa(std::uint64_t v, char* out) noexcept {
    const int len = u64toa_detail::exact_decimal_length(v);
    char* dst = out + len;

    // Peel 8 digits at a time (at most 2 iterations for 64-bit integers)
    while (v >= 100000000ULL) {
        const std::uint64_t q = v / 100000000ULL;
        const std::uint32_t r = static_cast<std::uint32_t>(v - q * 100000000ULL);
        v = q;
        dst -= 8;
        u64toa_detail::write_8digits(dst, r);
    }

    // Remaining value is strictly < 10^8, so all arithmetic narrows to 32-bit registers
    std::uint32_t v32 = static_cast<std::uint32_t>(v);
    if (v32 >= 10000U) {
        const std::uint32_t q = v32 / 10000U;
        const std::uint32_t r = v32 - q * 10000U;
        v32 = q;
        dst -= 4;
        u64toa_detail::write_4digits(dst, r);
    }
    if (v32 >= 100U) {
        const std::uint32_t q = v32 / 100U;
        const std::uint32_t r = v32 - q * 100U;
        v32 = q;
        dst -= 2;
        std::memcpy(dst, &u64toa_detail::kDigits100[r * 2U], 2);
    }
    if (v32 >= 10U) {
        std::memcpy(out, &u64toa_detail::kDigits100[v32 * 2U], 2);
    } else {
        out[0] = static_cast<char>('0' + v32);
    }
    return len;
}

// Out-of-line entry point defined in u64toa.cpp (for multi-TU linking and benchmarking)
int u64toa_outlined(std::uint64_t v, char* out) noexcept;

// Original starter implementation ("before") defined in u64toa.cpp for A/B benchmarking
int u64toa_baseline(std::uint64_t v, char* out) noexcept;
