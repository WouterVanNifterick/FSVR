// Import Audio: a sound file turned into a formant sequence the engine plays, after fseq-flash (Zach Archer,
// MIT, https://github.com/zkarcher/fseq-flash): per frame a pitch, and eight formants picked from the
// smoothed spectrum as the strongest peaks, their energy split between the voiced and the unvoiced
// operators by how periodic the frame is. The frame bytes use the engine's own scales, not fseq-flash's.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fsvr {

// Mono samples at the file's own rate. WAV and AIFF (dr_wav), MP3 (dr_mp3), Ogg Vorbis (stb_vorbis); MP4,
// M4A and AAC through the system's decoder (Media Foundation on Windows, Core Audio on macOS) or, on
// Linux, an ffmpeg on the PATH.
bool decodeAudio(const std::string& utf8Path, std::vector<float>& mono, double& rate, std::string& err);

// The Fseq's bulk data (32 header bytes, then 50 bytes a frame), named name, sized to the sound: the
// header's speed makes it play at the sound's own pace at 100 %, and its note is the one that plays the
// sound's own pitch. frames gets how many frames it uses.
std::vector<uint8_t> audioToFseq(const std::vector<float>& mono, double rate, const std::string& name, int& frames);

} // namespace fsvr
