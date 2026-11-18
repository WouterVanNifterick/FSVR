// patchfiles/format_vmem.cpp - a DX7 32-voice bank, F0 43 0n 09 <count> <32 x 128 packed> cs F7. Ours.
//
// Unpacked to VCED here, since the FS1R takes only VCED. The byte count is not trusted: the DX7 sends 20 00
// and the DX5 10 00 for the same 4096 bytes. A bad checksum is a warning, and bytes out of the DX7's ranges
// are clamped to them. An AMEM read before it gives each voice its supplement.
#include "dx7.h"
#include <cstring>

namespace fsvr::patchfiles {

bool readVmem(const Message& m, Reading& r) {
    if (!dxBulk(m, 0x09) || m.len < 6 + VMEM_SIZE + 2) return false;
    checkSum(m, 6, VMEM_SIZE, "VMEM", r);
    if (!(m.p[4] == 0x20 && m.p[5] == 0x00))
        r.warn(m, "VMEM byte count " + std::to_string(m.p[4]) + " " + std::to_string(m.p[5]) + " read as the 4096 bytes it holds");
    const auto whole = std::make_shared<const Bytes>(m.p, m.p + m.len);
    Bytes amem;
    amem.swap(r.pending.amem);
    int clamped = 0;
    for (int k = 0; k < 32; ++k) {
        uint8_t vced[VCED_SIZE], aced[ACED_SIZE];
        unpackRecord(m.p + 6 + RECORD * k, vced);
        clamped += clampVced(vced);
        const bool supplement = !amem.empty() && (amemToAced(amem.data() + AMEM_SLOT * k, aced), acedMatters(aced));
        Item it = dxVoice(vced, supplement ? aced : nullptr);
        it.at = m.at;
        it.len = m.len;
        it.slot = k;
        it.vmem = whole;
        r.bank.voices.push_back(std::move(it));
    }
    if (clamped) r.warn(m, std::to_string(clamped) + " VMEM voices had bytes out of the DX7's ranges, clamped");
    r.open = -1;
    return true;
}

static Bytes resummed(Bytes m) {
    m[6 + VMEM_SIZE] = checksum(m.data() + 6, VMEM_SIZE);
    return m;
}

} // namespace fsvr::patchfiles

namespace fsvr {

using namespace patchfiles;

std::vector<uint8_t> vmemRenamed(const Item& it, const std::string& name) {
    if (!it.vmem || it.slot < 0 || it.vmem->size() < 6 + VMEM_SIZE + 2) return it.vmem ? *it.vmem : Bytes{};
    Bytes m = *it.vmem;
    uint8_t* p = m.data() + 6 + RECORD * (size_t)it.slot + RECORD_NAME;
    for (size_t k = 0; k < NAME_LENGTH; ++k) {
        const char c = k < name.size() ? name[k] : ' ';
        p[k] = (uint8_t)(c >= 32 && c < 127 ? c : ' ');
    }
    return resummed(m);
}

std::vector<uint8_t> vmemBlanked(const Item& it) {
    if (!it.vmem || it.slot < 0 || it.vmem->size() < 6 + VMEM_SIZE + 2) return it.vmem ? *it.vmem : Bytes{};
    Bytes m = *it.vmem;
    uint8_t vced[VCED_SIZE];
    initVoice(vced);
    packRecord(vced, m.data() + 6 + RECORD * (size_t)it.slot);
    return resummed(m);
}

} // namespace fsvr
