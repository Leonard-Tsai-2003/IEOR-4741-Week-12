#pragma once
#include <cstdint>
// HW11 — a high-performance FIX parser (SOH-delimited tag=value; tags 11/55/54/38/44).
struct NewOrder {
    const char* clordid; int clordid_len;   // tag 11
    char symbol[16]; char side;             // tags 55, 54
    uint32_t qty; double price;             // tags 38, 44
};
// Single pass, no per-message allocation. Return false on a malformed message.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>

inline bool parse_new_order(const char* buf, int len, NewOrder& out) {
    const char SOH = '\x01';
    const char* p   = buf;
    const char* end = buf + len;
    bool id=false, sym=false, side=false, qty=false, px=false;

    while (p < end) {
        // --- tag: digits up to '=' ---
        int tag = 0;
        while (p < end && *p != '=') {
            if (*p < '0' || *p > '9') return false;   // malformed tag
            tag = tag * 10 + (*p - '0');
            ++p;
        }
        if (p >= end) return false;                    // no '=' → malformed
        ++p;                                           // consume '='

        // --- value: bytes up to SOH ---
        const char* v = p;
        while (p < end && *p != SOH) ++p;
        const int vlen = static_cast<int>(p - v);
        if (p < end) ++p;                              // consume SOH
            switch (tag) {
            case 11:                                   // ClOrdID — keep a view, no copy
                out.clordid = v; out.clordid_len = vlen; id = true;
                break;
            case 55: {                                 // Symbol — copy into fixed buffer
                int n = vlen < 15 ? vlen : 15;
                std::memcpy(out.symbol, v, n);
                out.symbol[n] = '\0';
                sym = true;
                break;
            }
            case 54:                                   // Side — single char ('1'=buy,'2'=sell)
                if (vlen >= 1) { out.side = v[0]; side = true; }
                break;
            case 38: {                                 // OrderQty — hand-rolled atoi
                uint32_t q = 0;
                for (int i = 0; i < vlen; ++i) {
                    if (v[i] < '0' || v[i] > '9') return false;
                    q = q * 10 + static_cast<uint32_t>(v[i] - '0');
                }
                out.qty = q; qty = true;
                break;
            }
            case 44:                                   // Price
                out.price = std::strtod(v, nullptr);   // stops at SOH; no allocation
                px = true;
                break;
            default:
                break;                                  // ignore 8/9/35/10 etc.
        }
    }
    return id && sym && side && qty && px;             // all required fields seen
}