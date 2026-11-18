#include "u64toa.hpp"

int u64toa_outlined(std::uint64_t v, char* out) noexcept {
    return u64toa(v, out);
}

int u64toa_baseline(std::uint64_t v, char* out) noexcept {
    char tmp[20];
    int n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < n; ++i)
        out[i] = tmp[n - 1 - i];
    return n;
}