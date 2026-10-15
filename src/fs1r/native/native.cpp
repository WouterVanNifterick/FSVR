// fs1r/native/native.cpp - the table of ported VOP3 programs. Ours.
#include "fs1r/native/native.h"
#include <cstring>

namespace fs1r::native {

// One line per ported program, in key order, as {"ins-m03", ins_m03},
const Program PROGRAMS[] = {
    {"dly-m00", dly_m00},
    {"dly-m08", dly_m08},
    {"flt", flt},
    {"rev", rev},
    {nullptr, nullptr},
};

const Program* find(const char* key) {
    for (const Program* p = PROGRAMS; p->key; p++)
        if (!std::strcmp(p->key, key)) return p;
    return nullptr;
}

}  // namespace fs1r::native
