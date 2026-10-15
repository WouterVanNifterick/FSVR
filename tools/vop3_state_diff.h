// The first field two VOP3 cores differ in, or "" when they hold the same state, bit for bit. Shared by
// vop3_jit_check (compiled against interpreted) and vop3_native_check (C++ programs against interpreted),
// so both mean the same thing by identical. The delay RAM is only compared when asked: it is 2 MB.
#pragma once
#include <cstring>
#include <string>
#include "fs1r/chips/yss236_core.h"

inline std::string vop3_state_diff(const yss236_core& a, const yss236_core& b, bool ram) {
    auto at = [](const char* name, const void* x, const void* y, size_t bytes, size_t elem) -> std::string {
        for (size_t i = 0; i < bytes / elem; i++)
            if (std::memcmp((const char*)x + i * elem, (const char*)y + i * elem, elem)) return std::string(name) + "[" + std::to_string(i) + "]";
        return "";
    };
    std::string w;
#define F(name, field) if (!(w = at(name, &a.field, &b.field, sizeof a.field, sizeof a.field[0])).empty()) return w;
#define S(field) if (std::memcmp(&a.field, &b.field, sizeof a.field)) return #field;
    F("r", r) F("m", m) F("t", t) F("out", out)
    S(acc) S(x) S(px) S(hasx) S(haspx) S(src) S(psrc) S(wlatch) S(haswl) S(idx) S(ptr) S(n) S(m_skip) S(m_flag) S(m_nhist) S(m_iqn)
    for (int i = 0; i < 4; i++) {
        const char* q[3] = {"m_rq", "m_dq", "m_tq"};
        const yss236_core::pend* pa[3] = {a.m_rq, a.m_dq, a.m_tq}; const yss236_core::pend* pb[3] = {b.m_rq, b.m_dq, b.m_tq};
        for (int k = 0; k < 3; k++)
            if (pa[k][i].reg != pb[k][i].reg || std::memcmp(&pa[k][i].v, &pb[k][i].v, 8)) return std::string(q[k]) + "[" + std::to_string(i) + "]";
    }
    for (int i = 0; i < a.m_iqn; i++)
        if (a.m_iq_at[i] != b.m_iq_at[i] || std::memcmp(&a.m_iq_v[i], &b.m_iq_v[i], 8)) return "m_iq[" + std::to_string(i) + "]";
    if (ram && !(w = at("ram", a.ram.data(), b.ram.data(), a.ram.size() * 8, 8)).empty()) return w;
    return "";
#undef F
#undef S
}
