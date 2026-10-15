// S32 audio bug test (user report: unplugging the pad raised the pitch of the sound for a few seconds).
// A simulation of src/app/main.cpp's sound path with an exact 48 kHz sound card: the machine makes one block of sound per frame (60.606 per second), a card
// callback takes 480 samples every 10 ms, and the program is frozen for a while at t = 5 s (what a slow game pad removal does on Windows), then catches up with up to
// 5 frames back to back, exactly like the main loop. It uses the real AudioRing, AreaResampler, ring_level_scale and RingGuard. The pitch error is the resampler
// scale's distance from 1 (2.5 % = about half a semitone, clearly audible on sound effects).
// Without the guard the test reproduces the bug (error 2.5 % for seconds); with it the error stays under 0.6 % and never more than 1 % for any time.
// Build: g++ -std=c++17 -O2 tests/test_audio_stall.cpp -o build/s32/test_audio_stall (see tests/test_audio_stall.sh)
#include <cmath>
#include <cstdio>
#include <vector>
#include "../src/audio/audio_out.h"
#include "../src/audio/audio_out.cpp"

struct Result {
    int underruns;
    double max_err, secs_over_1pct;
};
static Result simulate(double stall_ms, bool guarded, int catch_cap) {
    const double RATE = 48000, TICK = 3072000.0 / 50688.0;
    const size_t target = (size_t)(0.012 * RATE);
    AudioRing ring(1 << 15);
    AreaResampler rs(96000, RATE);
    RingGuard guard;
    std::vector<int16_t> chip(1584, 1000), out, tmp(480);
    double level_smooth = (double)target, next = 0, cons_t = 0;
    uint64_t underruns = 0;
    bool playing = false;
    Result r{0, 0, 0};
    const double stall_start = 5.0, stall_end = 5.0 + stall_ms / 1000;
    for (double t = 0; t < 40; t += 0.0005) {
        while (cons_t <= t) {
            cons_t += 0.010;
            if (playing) {
                size_t got = ring.read(tmp.data(), 480);
                if (got < 480) ++underruns;
            }
        }
        if (t >= stall_start && t < stall_end) continue;
        int steps = 0;
        bool burst = false;
        while (t >= next) {
            burst = burst || t >= next + 1 / TICK;
            bool write = true;
            if (guarded) {
                GuardDecision gd = guard.decide(ring.size(), underruns, target, burst);
                if (gd.pad_zeros) {
                    std::vector<int16_t> z(gd.pad_zeros, 0);
                    ring.write(z.data(), z.size());
                }
                if (gd.resync) level_smooth = (double)ring.size();
                write = gd.write;
            }
            if (write) {
                level_smooth += 0.02 * ((double)ring.size() - level_smooth);
                double sc = ring_level_scale((size_t)level_smooth, target);
                rs.set_scale(sc);
                out.clear();
                rs.process(chip.data(), chip.size(), out);
                ring.write(out.data(), out.size());
                if (!playing && ring.size() >= 2 * (size_t)(RATE / TICK)) playing = true;
                if (t > 5.0) {
                    double e = std::fabs(sc - 1);
                    if (e > r.max_err) r.max_err = e;
                    if (e > 0.01) r.secs_over_1pct += 1 / TICK;
                }
            }
            next += 1 / TICK;
            if (++steps >= catch_cap) {
                next = t + 1 / TICK;
                break;
            }
        }
    }
    r.underruns = (int)underruns;
    return r;
}

int main() {
    int failures = 0, checks = 0;
    auto expect = [&](bool ok, const char *what) {
        ++checks;
        if (!ok) {
            ++failures;
            std::printf("  FAIL: %s\n", what);
        }
    };
    Result base = simulate(0, true, 5);
    std::printf("no freeze, guard on : max pitch error %.2f %%, underruns %d\n", base.max_err * 100, base.underruns);
    expect(base.max_err < 0.005 && base.underruns == 0, "without a freeze the guard changes nothing (error under 0.5 %, no underruns)");
    Result bug = simulate(100, false, 5);
    std::printf("100 ms freeze, NO guard (the bug): max pitch error %.2f %% for %.1f s over 1 %%\n", bug.max_err * 100, bug.secs_over_1pct);
    expect(bug.max_err > 0.02 && bug.secs_over_1pct > 1.0, "the bug is reproduced without the guard (error above 2 % for more than 1 s)");
    const double stalls[] = {20, 40, 60, 100, 200, 300, 500, 1000, 3000};
    for (double s : stalls) {
        Result r = simulate(s, true, 5);
        std::printf("%5.0f ms freeze, guard on          : max pitch error %.2f %%, time over 1 %% = %.1f s, underruns while frozen %d\n", s, r.max_err * 100,
                    r.secs_over_1pct, r.underruns);
        char w[100];
        std::snprintf(w, sizeof w, "%.0f ms freeze: pitch error stays under 0.6 %% and never over 1 %%", s);
        expect(r.max_err < 0.006 && r.secs_over_1pct == 0.0, w);
    }
    Result sm = simulate(300, true, 1);
    std::printf("300 ms freeze, one frame per pass (smooth-like), guard on: max pitch error %.2f %%\n", sm.max_err * 100);
    expect(sm.max_err < 0.01, "one-frame-per-pass recovery stays under 1 %");
    { // S36: a burst of frames fills the queue far above its normal level: the new block must be dropped (limit 150 ms = 7200 samples), but not while the queue is in its normal range
        RingGuard g;
        const size_t target = 576, high = 7200;
        GuardDecision a = g.decide(3000, 0, target, false, high), b = g.decide(7201, 0, target, false, high), c = g.decide(7201, 0, target, false, 0);
        expect(a.write, "S36: a queue of 62 ms is written to normally");
        expect(!b.write && b.resync, "S36: a queue over 150 ms makes the guard drop the new block and re-set the level");
        expect(c.write, "S36: with the limit switched off (0) nothing changes for old callers");
    }
    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL test_audio_stall" : "PASS test_audio_stall", checks, failures);
    return failures ? 1 : 0;
}
