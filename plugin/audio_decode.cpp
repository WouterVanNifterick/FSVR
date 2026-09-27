// decodeAudio: the file formats Import Audio reads, each decoded to mono float at its own rate.
#include "audio_fseq.h"
#include "library.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#elif defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define STB_VORBIS_HEADER_ONLY
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "dr_wav.h"
#include "dr_mp3.h"
#include "stb_vorbis.c"   // its declarations; the implementation goes at the end, since it leaves short macros defined
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace fsvr {

static void mixDown(const float* in, size_t frames, int channels, std::vector<float>& mono) {
    mono.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        float s = 0;
        for (int c = 0; c < channels; ++c) s += in[i * (size_t)channels + (size_t)c];
        mono[i] = s / (float)std::max(channels, 1);
    }
}

// MP4, M4A and AAC: the system's decoder.
static bool decodeSystem(const std::string& path, std::vector<float>& mono, double& rate, std::string& err) {
#if defined(_WIN32)
    const std::wstring wpath = std::filesystem::u8path(path).wstring();
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool ok = false;
    if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) {
        IMFSourceReader* r = nullptr;
        IMFMediaType *want = nullptr, *got = nullptr;
        if (SUCCEEDED(MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &r)) && SUCCEEDED(MFCreateMediaType(&want))) {
            const DWORD audio = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
            r->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
            r->SetStreamSelection(audio, TRUE);
            want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
            if (SUCCEEDED(r->SetCurrentMediaType(audio, nullptr, want)) && SUCCEEDED(r->GetCurrentMediaType(audio, &got))) {
                const int ch = (int)MFGetAttributeUINT32(got, MF_MT_AUDIO_NUM_CHANNELS, 1);
                rate = MFGetAttributeUINT32(got, MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
                std::vector<float> all;
                for (;;) {
                    DWORD flags = 0;
                    IMFSample* s = nullptr;
                    if (FAILED(r->ReadSample(audio, 0, nullptr, &flags, nullptr, &s)) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                        if (s) s->Release();
                        break;
                    }
                    IMFMediaBuffer* b = nullptr;
                    if (s && SUCCEEDED(s->ConvertToContiguousBuffer(&b))) {
                        BYTE* p = nullptr;
                        DWORD len = 0;
                        if (SUCCEEDED(b->Lock(&p, nullptr, &len))) {
                            all.insert(all.end(), (const float*)p, (const float*)p + len / sizeof(float));
                            b->Unlock();
                        }
                        b->Release();
                    }
                    if (s) s->Release();
                }
                mixDown(all.data(), all.size() / (size_t)std::max(ch, 1), ch, mono);
                ok = !mono.empty();
            }
        }
        if (got) got->Release();
        if (want) want->Release();
        if (r) r->Release();
        MFShutdown();
    }
    if (SUCCEEDED(co)) CoUninitialize();
    if (!ok) err = "Windows could not decode " + path;
    return ok;
#elif defined(__APPLE__)
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)path.c_str(), (CFIndex)path.size(), false);
    ExtAudioFileRef f = nullptr;
    const bool opened = url && ExtAudioFileOpenURL(url, &f) == noErr;
    if (url) CFRelease(url);
    if (!opened) {
        err = "macOS could not open " + path;
        return false;
    }
    AudioStreamBasicDescription in = {}, c = {};
    UInt32 size = sizeof in;
    ExtAudioFileGetProperty(f, kExtAudioFileProperty_FileDataFormat, &size, &in);
    const int ch = std::max<int>(1, (int)in.mChannelsPerFrame);
    c.mSampleRate = in.mSampleRate;
    c.mFormatID = kAudioFormatLinearPCM;
    c.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    c.mChannelsPerFrame = (UInt32)ch;
    c.mBitsPerChannel = 32;
    c.mBytesPerFrame = c.mBytesPerPacket = 4 * (UInt32)ch;
    c.mFramesPerPacket = 1;
    ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof c, &c);
    std::vector<float> all, buf(4096 * (size_t)ch);
    for (;;) {
        UInt32 frames = 4096;
        AudioBufferList abl;
        abl.mNumberBuffers = 1;
        abl.mBuffers[0] = {(UInt32)ch, (UInt32)(buf.size() * sizeof(float)), buf.data()};
        if (ExtAudioFileRead(f, &frames, &abl) != noErr || frames == 0) break;
        all.insert(all.end(), buf.begin(), buf.begin() + (size_t)frames * (size_t)ch);
    }
    ExtAudioFileDispose(f);
    rate = in.mSampleRate;
    mixDown(all.data(), all.size() / (size_t)ch, ch, mono);
    if (mono.empty()) err = "macOS could not decode " + path;
    return !mono.empty();
#else
    // ponytail: no AAC decoder of our own on Linux; ffmpeg is on most desktops, bundle one if users ask.
    const char* argv[] = {"ffmpeg", "-v", "error", "-i", path.c_str(), "-f", "f32le", "-ac", "1", "-ar", "44100", "-", nullptr};
    int fd[2];
    if (pipe(fd) != 0) return false;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fd[1], 1);
    posix_spawn_file_actions_addclose(&fa, fd[0]);
    pid_t pid = 0;
    const bool ok = posix_spawnp(&pid, "ffmpeg", &fa, nullptr, const_cast<char* const*>(argv), environ) == 0;
    posix_spawn_file_actions_destroy(&fa);
    close(fd[1]);
    std::vector<char> bytes;
    char buf[65536];
    for (ssize_t n; ok && (n = read(fd[0], buf, sizeof buf)) != 0;) {
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) break;
        bytes.insert(bytes.end(), buf, buf + n);
    }
    close(fd[0]);
    int status = 0;
    if (ok) while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (!ok || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || bytes.size() < sizeof(float)) {
        err = ok ? "ffmpeg could not decode " + path : "MP4 and AAC need ffmpeg on Linux";
        return false;
    }
    mono.resize(bytes.size() / sizeof(float));
    std::memcpy(mono.data(), bytes.data(), mono.size() * sizeof(float));
    rate = 44100;
    return true;
#endif
}

bool decodeAudio(const std::string& path, std::vector<float>& mono, double& rate, std::string& err) {
    std::vector<uint8_t> f;
    if (!readFile(path, f) || f.size() < 12) {
        err = "cannot read " + path;
        return false;
    }
    auto is = [&](size_t at, const char* tag) { return f.size() >= at + std::strlen(tag) && std::memcmp(f.data() + at, tag, std::strlen(tag)) == 0; };
    if (is(0, "RIFF") || is(0, "RF64") || is(0, "FORM") || is(0, "riff")) {   // WAV, AIFF, AIFC, W64
        drwav w;
        if (!drwav_init_memory(&w, f.data(), f.size(), nullptr)) {
            err = path + " is not a WAV or AIFF file this can read";
            return false;
        }
        std::vector<float> all((size_t)w.totalPCMFrameCount * w.channels);
        const drwav_uint64 got = drwav_read_pcm_frames_f32(&w, w.totalPCMFrameCount, all.data());
        rate = w.sampleRate;
        mixDown(all.data(), (size_t)got, (int)w.channels, mono);
        drwav_uninit(&w);
        return !mono.empty();
    }
    if (is(0, "OggS")) {
        int ch = 0, sr = 0;
        short* out = nullptr;
        const int frames = stb_vorbis_decode_memory(f.data(), (int)f.size(), &ch, &sr, &out);
        if (frames <= 0 || !out) {
            err = path + " is not an Ogg Vorbis file this can read";
            return false;
        }
        std::vector<float> all((size_t)frames * (size_t)ch);
        for (size_t i = 0; i < all.size(); ++i) all[i] = out[i] / 32768.0f;
        free(out);
        rate = sr;
        mixDown(all.data(), (size_t)frames, ch, mono);
        return true;
    }
    if (is(4, "ftyp")) return decodeSystem(path, mono, rate, err);   // MP4, M4A
    std::string ext = std::filesystem::u8path(path).extension().u8string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == ".aac" || ext == ".m4a" || ext == ".mp4") return decodeSystem(path, mono, rate, err);
    drmp3_config cfg;   // MP3: an ID3 tag or a frame sync first
    drmp3_uint64 frames = 0;
    float* out = drmp3_open_memory_and_read_pcm_frames_f32(f.data(), f.size(), &cfg, &frames, nullptr);
    if (!out || !frames) {
        err = path + " is not a WAV, AIFF, MP3, Ogg Vorbis or MP4 file";
        return false;
    }
    rate = cfg.sampleRate;
    mixDown(out, (size_t)frames, (int)cfg.channels, mono);
    drmp3_free(out, nullptr);
    return true;
}

} // namespace fsvr

#undef STB_VORBIS_HEADER_ONLY
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "stb_vorbis.c"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
