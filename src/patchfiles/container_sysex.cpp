// patchfiles/container_sysex.cpp - a sysex stream: a .syx, and an .fs1, which is one performance and its
// voices as the same messages. Ours. Bytes outside an F0..F7, a librarian's header or fill, are not read.
#include "reading.h"

namespace fsvr::patchfiles {

std::vector<Message> messages(const Bytes& b) {
    std::vector<Message> out;
    const size_t n = b.size();
    for (size_t i = 0; i < n;) {
        if (b[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < n && b[j] != 0xF7) ++j;
        if (j >= n) break;
        out.push_back({b.data() + i, j - i + 1, i});
        i = j + 1;
    }
    return out;
}

} // namespace fsvr::patchfiles
