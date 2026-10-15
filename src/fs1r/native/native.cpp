// fs1r/native/native.cpp - the table of ported VOP3 programs. Ours.
#include "fs1r/native/native.h"
#include <cstring>

namespace fs1r::native {

// One line per ported program, in key order, as {"ins-m03", ins_m03},
const Program PROGRAMS[] = {
    {"aux", aux},
    {"dly-m00", dly_m00},
    {"dly-m08", dly_m08},
    {"flt", flt},
    {"ins-m00", ins_m00},
    {"ins-m01", ins_m01},
    {"ins-m02", ins_m02},
    {"ins-m03", ins_m03},
    {"ins-m04", ins_m04},
    {"ins-m05", ins_m05},
    {"rev", rev},
    {"var-m00", var_m00},
    {"var-m01", var_m01},
    {"var-m02", var_m02},
    {"var-m03", var_m03},
    {"var-m04", var_m04},
    {"var-m05", var_m05},
    {"var-m06", var_m06},
    {"var-m07", var_m07},
    {"var-m08", var_m08},
    {"var-m09", var_m09},
    {"var-m10", var_m10},
    {nullptr, nullptr},
};

const Program* find(const char* key) {
    for (const Program* p = PROGRAMS; p->key; p++)
        if (!std::strcmp(p->key, key)) return p;
    return nullptr;
}

}  // namespace fs1r::native
