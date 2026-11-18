// patchfiles/reading.h - what the dispatcher, the containers and the formats share. Ours.
#pragma once
#include "patchfiles.h"

namespace fsvr::patchfiles {

using Bytes = std::vector<uint8_t>;

// One F0..F7 of a sysex stream, and where it starts.
struct Message {
    const uint8_t* p;
    size_t len, at;
};

// One file being read: the bank it fills, the report, what waits for a later message, and the
// performance the bulks right after it belong to.
struct Reading {
    Bank& bank;
    Report& report;
    Pending& pending;
    int open = -1;
    size_t acedAt = SIZE_MAX;            // the pending ACED's place in this file; SIZE_MAX when it came in an earlier one
    void warn(const Message& m, const std::string& what);
};

// Containers: a file to a sysex stream. The first two say whether the file is theirs and rewrap it into out.
bool smfStream(const Bytes& file, Bytes& out, Report& report);          // container_smf.cpp
bool headerlessStream(const Bytes& file, Bytes& out, Report& report);   // container_headerless.cpp
std::vector<Message> messages(const Bytes& stream);                     // container_sysex.cpp: a .syx, an .fs1

// Formats: each takes the messages that are its own and says whether it did.
bool readFs1r(const Message& m, Reading& r);   // format_fs1r.cpp
bool readAced(const Message& m, Reading& r);   // format_aced.cpp
bool readVced(const Message& m, Reading& r);   // format_vced.cpp
bool readAmem(const Message& m, Reading& r);   // format_amem.cpp, with the DX7II bank select
bool readVmem(const Message& m, Reading& r);   // format_vmem.cpp

// A Yamaha DX bulk: F0 43 0n <format> <count hi> <count lo>, whatever the device number.
inline bool dxBulk(const Message& m, uint8_t format) { return m.len >= 8 && m.p[1] == 0x43 && (m.p[2] & 0xF0) == 0 && m.p[3] == format; }

} // namespace fsvr::patchfiles
