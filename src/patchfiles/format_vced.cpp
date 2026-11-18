// patchfiles/format_vced.cpp - a DX7 single voice, F0 43 0n 00 01 1B <155> cs F7, with the ACED before it. Ours.
#include "dx7.h"

namespace fsvr::patchfiles {

bool readVced(const Message& m, Reading& r) {
    if (!dxBulk(m, 0x00) || m.p[4] != 0x01 || m.p[5] != 0x1B || m.len < 6 + VCED_SIZE + 2) return false;
    checkSum(m, 6, VCED_SIZE, "VCED", r);
    const bool paired = !r.pending.aced.empty();
    r.bank.voices.push_back(dxVoice(m.p + 6, paired ? r.pending.aced.data() : nullptr));
    Item& it = r.bank.voices.back();
    it.at = paired && r.acedAt != SIZE_MAX ? r.acedAt : m.at;   // a voice's bytes take its ACED in with it
    it.len = m.at + m.len - it.at;
    r.pending.aced.clear();
    r.acedAt = SIZE_MAX;
    r.open = -1;
    return true;
}

} // namespace fsvr::patchfiles
