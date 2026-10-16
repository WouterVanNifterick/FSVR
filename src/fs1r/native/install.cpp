// fs1r/native/install.cpp - which of the C++ programs to run on a chip, from what its registers hold.
//
// A core's program is five words a step, written by the host from the EPROM's tables. Every program in
// native/ is one of those EPROM programs rewritten in C++, named by the window it belongs to and the type
// msb it was filed under, so which programs to run is a question about the words the chip currently
// holds. This file answers it by comparing them, the way tools/vop3_native_check does for one program at
// a time, and hands yss236_core::natives the ranges to run.
//
// Nothing here decides what a step computes. A program we did not port, and a window whose words are not
// one of the EPROM's at all, simply get no range and run on the interpreter, which is the same result at
// the cost of speed and nothing else.

#include "fs1r/native/native.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "fs1r/firmware/vop3host/effect_host.h"

namespace fs1r::native {

namespace host = fs1r::vop3host;

namespace {

using Runs = std::vector<yss236_core::native_run>;

// The key a window's program for this type msb is filed under, so ins-mNN and var-mNN and dly-mNN
const char* key_for(int window, int msb)
{
    static char key[8];
    std::snprintf(key, sizeof key, "%s-m%02d", window == host::INSERTION ? "ins"
                                       : window == host::REVERB_DELAY ? "dly" : "var", msb);
    return key;
}

// How many programs the EPROM holds for a window
int program_count(int window)
{
    switch (window)
    {
        case host::INSERTION:    return int(std::size(host::rom::INSERTION_PROGRAMS))    / host::WINDOWS[window].nsteps;
        case host::REVERB_DELAY: return int(std::size(host::rom::REVERB_DELAY_PROGRAMS)) / host::WINDOWS[window].nsteps;
        case host::VARIATION:    return int(std::size(host::rom::VARIATION_PROGRAMS))    / host::WINDOWS[window].nsteps;
        default: return 0;
    }
}

// Whether the window's steps hold the EPROM's program for this type msb, word for word
bool holds(const yss236_core& c, int window, int msb)
{
    const host::Window& w = host::WINDOWS[window];
    const uint16_t (*programs)[5] = w.programs + msb * w.nsteps;
    for (int i = 0; i < w.nsteps; i++)
        if (std::memcmp(c.prog[w.steps[i]], programs[i], sizeof c.prog[0]))
            return false;
    return true;
}

// The covered steps as runs of consecutive ones, each handed to the program. Steps already claimed by an
// earlier program stay with it: the reverb block's 94 steps are scattered inside the base program's, so a
// later window must not take them back.
Runs runs_of(bool const covered[], Run run)
{
    Runs runs;
    for (int st = 0; st < yss236_core::STEPS; st++)
        if (covered[st]) {
            if (!runs.empty() && runs.back().to == st)
                runs.back().to++;
            else
                runs.push_back({st, st + 1, run});
        }
    return runs;
}

// One window's steps as a coverage map, leaving out what an earlier program already has
void mark_window(bool taken[], bool covered[], int window)
{
    const host::Window& w = host::WINDOWS[window];
    for (int i = 0; i < w.nsteps; i++)
        if (!taken[w.steps[i]])
            covered[w.steps[i]] = true;
}

}  // namespace

// The filter chip runs one program for every type it has, so all 512 steps are ours.
void install_filter(yss236_core& c)
{
    c.natives = Runs{{0, yss236_core::STEPS, flt}};
}

// The effect chip's steps, as far as we have C++ for what is loaded on it.
//
//   base      every step no window owns
//   rev       the reverb block, which has no program of its own and is 94 steps of the base program's
//   aux       the aux window, which has one program and so always holds it
//   ins/var/dly  whichever of the EPROM's programs the window holds, where we ported that one
//
// The windows interleave with each other and with the base program, so the ranges come out in step order
// with the interpreter between them, which is what yss236_core::native_pass expects.
void install_effect(yss236_core& c)
{
    static const int WINDOWS[3] = {host::INSERTION, host::REVERB_DELAY, host::VARIATION};

    // Step order is the only order native_pass accepts, and the windows do not come in it: the reverb's
    // steps sit between the base program's, and the base program's between the reverb's. So each program
    // takes the steps no earlier one claimed, and the runs are sorted at the end.
    bool taken[yss236_core::STEPS] = {};
    Runs runs;

    auto take = [&](bool covered[], Run run) {
        Runs part = runs_of(covered, run);
        for (yss236_core::native_run const& r : part)
            for (int st = r.from; st < r.to; st++) taken[st] = true;
        runs.insert(runs.end(), part.begin(), part.end());
    };

    bool covered[yss236_core::STEPS];

    // The base program is what no window owns.
    std::fill_n(covered, yss236_core::STEPS, true);
    for (int window = 0; window < 5; window++)
        for (int i = 0; i < host::WINDOWS[window].nsteps; i++)
            covered[host::WINDOWS[window].steps[i]] = false;
    take(covered, base);

    // The reverb block has no program of its own: its 94 steps are part of the base program's, and the
    // aux window has one program, so it always holds it.
    std::fill_n(covered, yss236_core::STEPS, false);
    mark_window(taken, covered, host::REVERB);
    take(covered, rev);

    std::fill_n(covered, yss236_core::STEPS, false);
    mark_window(taken, covered, host::AUX);
    take(covered, aux);

    // The other three windows run whichever of the EPROM's programs they hold, and ours only where we
    // ported that one.
    for (int window : WINDOWS)
        for (int msb = 0; msb < program_count(window); msb++)
        {
            const Program* p = find(key_for(window, msb));
            if (!p || !holds(c, window, msb)) continue;
            std::fill_n(covered, yss236_core::STEPS, false);
            mark_window(taken, covered, window);
            take(covered, p->run);
            break;
        }

    std::sort(runs.begin(), runs.end(), [](yss236_core::native_run const& a, yss236_core::native_run const& b) { return a.from < b.from; });
    c.natives = std::move(runs);
}

}  // namespace fs1r::native