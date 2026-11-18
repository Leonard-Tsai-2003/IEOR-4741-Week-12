# Fast `uint64_t` to ASCII Serialization (`u64toa`)

**Course:** IEOR 4741 — Week 12 (Serialization & Low-Level Optimization)  
**Test Environment:** Apple M2, 8 GB RAM (`c++` / `g++ -std=c++17`)

---

## Overview

This repository implements a zero-allocation, locale-independent `uint64_t`-to-ASCII decimal serializer (`u64toa`) designed to outperform `std::to_string` and `snprintf` across all 64-bit unsigned integer magnitudes ($0$ through $2^{64} - 1$).

### Design Goals
- **Zero Heap & Zero Locale Overhead:** Writes directly into caller-provided buffers without `std::string` dynamic allocations, format-string parsing, or C/C++ locale locking.
- **Full Edge-Case Correctness:** Verified on `0`, single-digit integers, powers of $10$, `UINT64_MAX` ($18{,}446{,}744{,}073{,}709{,}551{,}615$), and 700,000 total fuzz-tested values.
- **Low Cycle Latency Across All Magnitudes:** Achieves **$3.46\text{--}11.51\text{ cycles/op}$** across $1\text{--}20$ digits ($8.55\text{ cycles/op}$ on a uniform mixed workload), outperforming `std::to_string` by **$2.74\times\text{--}6.39\times$** and `snprintf` by **$10.85\times\text{--}27.56\times$**.

---

## Repository Structure

```text
IEOR-4741-Week-12-main/
├── include/
│   └── u64toa.hpp            # Core fast uint64_t -> ASCII implementation
├── u64toa.cpp                # Translation unit wrapper
├── tests/
│   └── u64toa_test.cpp       # Edge-case & 200k fuzz correctness tests + ns/op check
├── bench_u64toa.cpp          # Cycle-level benchmark vs Before, std::to_string, and snprintf
└── Makefile                  # Build targets (make u64toa, etc.)
```

---

## Build & Run Instructions

### 1. Correctness Suite & `ns/op` Check (`-O2`)
```bash
make u64toa
# Equivalent command:
c++ -std=c++17 -O2 -pthread -Iinclude -Itests tests/u64toa_test.cpp -o /tmp/u64toa && /tmp/u64toa
```

### 2. Cycle-Level Benchmark Across Magnitudes (`-O3 -march=native`)
```bash
g++ -O3 -std=c++17 -march=native -Wall -Wextra -Iinclude bench_u64toa.cpp u64toa.cpp -o /tmp/bench_u64toa && /tmp/bench_u64toa
```

---

## Correctness Verification

Both the unit test runner and the benchmark harness validate exact byte-for-byte equivalence against `std::to_string` before recording timings:

- **`tests/u64toa_test.cpp`:**
  - `RESULT|u64toa_edges|pass|0, small, 10-digit, and UINT64_MAX correct`
  - `RESULT|u64toa_fuzz|pass|200k pseudo-random values match std::to_string`
- **`bench_u64toa.cpp`:**
  - `Correctness check PASSED (all edge cases + 500k fuzz values).`

---

## Performance Benchmarks (Apple M2, 8 GB)

### Latency Comparison (`make u64toa`, `-O2`)

| Metric | Value | Speedup vs `std::to_string` |
| :--- | ---: | ---: |
| `u64toa_ns_per_op` | `8.81033 ns/op` | **$1.39\times$** |
| `std_to_string_ns_per_op` | `12.2894 ns/op` | $1.00\times$ (baseline) |

### Cycle-Level Comparison (`bench_u64toa`, `-O3 -march=native`)

*Note: Timer derived from `std::chrono::steady_clock` at $3.0\text{ GHz}$ (`non-monotonic/virtualized TSC detected`).*

| Magnitude | Before | After | `std::to_string` | `snprintf` | vs Before | vs `std::to_string` | vs `snprintf` |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `0` (1 digit) | 6.70 cy | **3.76 cy** | 16.90 cy | 90.30 cy | **1.78x** | **4.49x** | **24.00x** |
| `7` (1 digit) | 5.90 cy | **3.46 cy** | 15.45 cy | 90.05 cy | **1.70x** | **4.46x** | **26.01x** |
| `42` (2 digits) | 7.95 cy | **4.54 cy** | 18.10 cy | 94.11 cy | **1.75x** | **3.99x** | **20.74x** |
| `12,345` (5 digits) | 17.24 cy | **3.76 cy** | 24.02 cy | 103.70 cy | **4.58x** | **6.39x** | **27.56x** |
| `1,000,000,000` (10 digits) | 20.10 cy | **5.44 cy** | 22.02 cy | 102.78 cy | **3.70x** | **4.05x** | **18.90x** |
| `123,456,789,012,345` (15 digits) | 32.24 cy | **7.52 cy** | 28.29 cy | 114.81 cy | **4.28x** | **3.76x** | **15.26x** |
| `1,000,000,000,000,000,000` (19d) | 40.44 cy | **10.36 cy** | 28.99 cy | 122.95 cy | **3.90x** | **2.80x** | **11.86x** |
| `UINT64_MAX` (20 digits) | 42.92 cy | **11.51 cy** | 31.56 cy | 124.86 cy | **3.73x** | **2.74x** | **10.85x** |
| **Mixed (uniform 1..20 digits)** | **23.70 cy** | **8.55 cy** | **27.66 cy** | **110.49 cy** | **2.77x** | **3.24x** | **12.92x** |

### Raw Benchmark Output
```text
Magnitude                        |    Before |     After | to_string |  snprintf | vsBefore | vsString | vsSnprtf
---------------------------------+-----------+-----------+-----------+-----------+----------+----------+----------
0 (1 digit)                      |   6.70 cy |   3.76 cy |  16.90 cy |  90.30 cy |    1.78x |    4.49x |   24.00x
7 (1 digit)                      |   5.90 cy |   3.46 cy |  15.45 cy |  90.05 cy |    1.70x |    4.46x |   26.01x
42 (2 digits)                    |   7.95 cy |   4.54 cy |  18.10 cy |  94.11 cy |    1.75x |    3.99x |   20.74x
12,345 (5 digits)                |  17.24 cy |   3.76 cy |  24.02 cy | 103.70 cy |    4.58x |    6.39x |   27.56x
1,000,000,000 (10 digits)        |  20.10 cy |   5.44 cy |  22.02 cy | 102.78 cy |    3.70x |    4.05x |   18.90x
123,456,789,012,345 (15 digits)  |  32.24 cy |   7.52 cy |  28.29 cy | 114.81 cy |    4.28x |    3.76x |   15.26x
1,000,000,000,000,000,000 (19d)  |  40.44 cy |  10.36 cy |  28.99 cy | 122.95 cy |    3.90x |    2.80x |   11.86x
UINT64_MAX (20 digits)           |  42.92 cy |  11.51 cy |  31.56 cy | 124.86 cy |    3.73x |    2.74x |   10.85x
---------------------------------+-----------+-----------+-----------+-----------+----------+----------+----------
Mixed (uniform 1..20 digits)     |  23.70 cy |   8.55 cy |  27.66 cy | 110.49 cy |    2.77x |    3.24x |   12.92x
```

---

## Single Change That Helped Most

The single biggest performance improvement came from **batching digits two at a time using a 200-byte lookup table (`"00"` through `"99"`) paired with direct-to-buffer placement via fast bit-length digit counting (`__builtin_clzll` + powers-of-10 table)**. Rather than executing one expensive 64-bit division and modulo per character (`n / 10` and `n % 10`), writing into a temporary stack buffer in reverse, and then performing a second copy pass forward, dividing by $100$ and indexing a contiguous two-character table halves the number of loop iterations and dependency-chain divisions while emitting two ASCII bytes per memory store directly at their final output offsets. On a 5-digit integer (`12,345`), this reduced execution time from `17.24 cy` to `3.76 cy` ($4.58\times$ speedup), and on `UINT64_MAX` (20 digits), from `42.92 cy` to `11.51 cy` ($3.73\times$ speedup), completely eliminating heap allocation, locale overhead, and intermediate buffer copies.