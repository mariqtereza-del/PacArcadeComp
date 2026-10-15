// S25 test of src/audio/wsg.{h,cpp}. Usage: test_wsg pacman.zip [out_prefix]     (writes out_prefix + "start_jingle.wav" and "a440.wav" when a prefix is given)
// Part A: formulas: voice register decoding, accumulator arithmetic, volume, mixing, enable bit, chunk independence.
// Part B: register 12C6h on voice 0 measures 440 Hz +/- 1% (Goertzel scan of the rendered samples; zero crossings as a second measure).
// Part C: U3: sample-by-sample comparison with a second description (floating-point phase in whole cycles, MAME's divide by 128*voices) over random register sets.
// Part D: the start jingle: run the C++ machine with coin and start, capture the sound registers every frame, render 1,584 samples per frame to a WAV,
//         and check: every note value is an entry of the ROM's note table (16 bytes at 3BB9, shifted by the octave), and the pitch measured in the
//         rendered audio of each steady note equals value * 96000 / 2^20 Hz within 1%.
// Build: g++ -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_wsg.cpp src/audio/wsg.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp miniz*.o
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "../src/audio/wsg.h"
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

static void write_wav(const std::string &fn, const std::vector<int16_t> &s) {
    FILE *o = std::fopen(fn.c_str(), "wb");
    if (!o) return;
    auto u32 = [&](uint32_t v) {
        uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
        std::fwrite(b, 1, 4, o);
    };
    auto u16 = [&](uint16_t v) {
        uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
        std::fwrite(b, 1, 2, o);
    };
    uint32_t bytes = (uint32_t)s.size() * 2;
    std::fwrite("RIFF", 1, 4, o);
    u32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, o);
    u32(16);
    u16(1);
    u16(1);
    u32(Wsg::kRate);
    u32(Wsg::kRate * 2);
    u16(2);
    u16(16);
    std::fwrite("data", 1, 4, o);
    u32(bytes);
    for (int16_t v : s) u16((uint16_t)v);
    std::fclose(o);
}

// power of the signal at frequency hz (Goertzel), x = samples
static double goertzel(const std::vector<int16_t> &x, size_t from, size_t n, double hz) {
    double w = 2 * 3.14159265358979323846 * hz / Wsg::kRate, c = 2 * std::cos(w), s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i) {
        double s = x[from + i] + c * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
// fundamental by autocorrelation (mean removed), lag search around the expected period; returns Hz (0 if not found)
static double pitch_by_autocorrelation(const std::vector<int16_t> &x, double expect_hz) {
    size_t n = x.size();
    double mean = 0;
    for (int16_t v : x) mean += v;
    mean /= n;
    double P = Wsg::kRate / expect_hz;
    int lo = (int)(P * 0.9), hi = (int)(P * 1.1) + 1;
    if ((size_t)hi * 2 >= n) return 0;
    auto ac = [&](int L) {
        double a = 0;
        for (size_t i = 0; i + L < n; ++i) a += (x[i] - mean) * (x[i + L] - mean);
        return a / (double)(n - L);
    };
    int best = lo;
    double bv = -1e30;
    std::vector<double> v(hi + 2);
    for (int L = lo - 1; L <= hi + 1; ++L) {
        v[L - (lo - 1)] = ac(L);
        if (L >= lo && L <= hi && v[L - (lo - 1)] > bv) {
            bv = v[L - (lo - 1)];
            best = L;
        }
    }
    double a = v[best - 1 - (lo - 1)], b = v[best - (lo - 1)], c = v[best + 1 - (lo - 1)];
    double d = a - 2 * b + c, off = d != 0 ? 0.5 * (a - c) / d : 0; // parabolic refinement
    return Wsg::kRate / (best + off);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_wsg pacman.zip [out_prefix]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    const uint8_t *prom = roms.sound1.data();
    std::string out = argc > 2 ? argv[2] : "";

    // ---------------- waveform PROM facts
    expect(roms.sound1.size() == 256, "waveform PROM (82s126.1m) is 256 bytes");
    {
        bool nib = true;
        for (int i = 0; i < 256; ++i)
            if (prom[i] > 0x0F) nib = false;
        std::printf("  waveform PROM: all values 0-15: %s\n", nib ? "yes" : "NO (only the low nibble is used)");
    }

    // ---------------- Part A: formulas
    {
        Wsg w(prom);
        uint8_t r[32] = {0};
        r[0x10] = 6;
        r[0x11] = 0xC;
        r[0x12] = 2;
        r[0x13] = 1;
        r[0x14] = 0;
        r[0x05] = 9;
        r[0x15] = 0xF; // voice 0: 0x12C6, waveform 9 -> 1
        r[0x16] = 0xF;
        r[0x17] = 0x8;
        r[0x18] = 0x5;
        r[0x19] = 0x2;
        r[0x0A] = 3;
        r[0x1A] = 7; // voice 1: nibbles F,8,5,2 -> 0x2 5 8 F0 = 0x258F0? see below
        r[0x1B] = 1;
        r[0x1C] = 2;
        r[0x1D] = 3;
        r[0x1E] = 4;
        r[0x0F] = 7;
        r[0x1F] = 1;
        w.load_registers(r);
        expect(w.frequency(0) == 0x12C6, "voice 0 frequency from five nibbles = 0x12C6");
        expect(w.frequency(1) == (0xFu << 4 | 0x8u << 8 | 0x5u << 12 | 0x2u << 16), "voice 1 frequency: nibbles shifted by 4, 8, 12, 16 (the lowest nibble is 0)");
        expect(w.frequency(2) == (1u << 4 | 2u << 8 | 3u << 12 | 4u << 16), "voice 2 frequency decoded the same way");
        expect((w.frequency(1) & 15) == 0 && (w.frequency(2) & 15) == 0, "voices 1 and 2 have no lowest nibble");
        expect(w.waveform(0) == 1 && w.waveform(1) == 3 && w.waveform(2) == 7, "waveform select = low 3 bits (9 -> 1)");
        expect(w.volume(0) == 15 && w.volume(1) == 7 && w.volume(2) == 1, "volumes");
        w.write(0x10, 0xF6);
        expect(w.frequency(0) == 0x12C6, "only the low nibble of a written byte counts");
        // accumulators, mixing, enable
        Wsg a(prom);
        a.load_registers(r);
        a.set_enabled(false);
        std::vector<int16_t> buf(1000);
        a.render(buf.data(), 100);
        bool silent = true;
        for (int16_t v : buf)
            if (v) silent = false;
        expect(silent && a.accumulator(0) == 0, "sound disabled (5001 = 0): silence and the accumulators stand still");
        a.set_enabled(true);
        a.render(buf.data(), 1000);
        expect(a.accumulator(0) == (1000u * 0x12C6u) % 0x100000u, "accumulator after 1000 ticks = 1000 * frequency mod 2^20");
        Wsg b(prom);
        b.load_registers(r);
        b.set_enabled(true);
        std::vector<int16_t> one(5000), chunks(5000);
        b.render(one.data(), 5000);
        Wsg c(prom);
        c.load_registers(r);
        c.set_enabled(true);
        size_t p = 0;
        for (int step : {1, 7, 100, 1892, 3000}) {
            c.render(chunks.data() + p, step);
            p += step;
        }
        expect(p == 5000 && one == chunks, "rendering in pieces gives the same samples as one block");
        // volume 0 -> silence, volume scaling, single voice amplitude
        Wsg d(prom);
        uint8_t q[32] = {0};
        q[0x10] = 6;
        q[0x11] = 0xC;
        q[0x12] = 2;
        q[0x13] = 1;
        q[0x05] = 0;
        q[0x15] = 15;
        d.load_registers(q);
        d.set_enabled(true);
        std::vector<int16_t> s(Wsg::kRate);
        d.render(s.data(), s.size());
        int mx = -99999, mn = 99999;
        for (int16_t v : s) {
            if (v > mx) mx = v;
            if (v < mn) mn = v;
        }
        int pmax = -99, pmin = 99;
        for (int i = 0; i < 32; ++i) {
            int v = Wsg::waveform_sample(prom, 0, i);
            if (v > pmax) pmax = v;
            if (v < pmin) pmin = v;
        }
        std::printf("  voice 0, waveform 0, volume 15: sample range %d .. %d (PROM peak values %d .. %d times 15 /384 * 32767)\n", mn, mx, pmin, pmax);
        expect(mx == pmax * 15 * 32767 / 384 && mn == pmin * 15 * 32767 / 384, "peak output = (peak sample - 8) * volume / 384 of full scale");
        q[0x15] = 0;
        d.load_registers(q);
        d.render(s.data(), 500);
        bool z = true;
        for (int i = 0; i < 500; ++i)
            if (s[i]) z = false;
        expect(z, "volume 0 gives silence");
        // two voices add
        Wsg e1(prom), e2(prom), e3(prom);
        uint8_t t1[32] = {0}, t2[32] = {0}, t3[32] = {0};
        t1[0x11] = 5;
        t1[0x12] = 3;
        t1[0x05] = 2;
        t1[0x15] = 9;
        t2[0x17] = 6;
        t2[0x18] = 2;
        t2[0x0A] = 4;
        t2[0x1A] = 5;
        for (int i = 0; i < 32; ++i) t3[i] = t1[i] | t2[i];
        e1.load_registers(t1);
        e2.load_registers(t2);
        e3.load_registers(t3);
        e1.set_enabled(true);
        e2.set_enabled(true);
        e3.set_enabled(true);
        std::vector<int16_t> s1(3000), s2(3000), s3(3000);
        e1.render(s1.data(), 3000);
        e2.render(s2.data(), 3000);
        e3.render(s3.data(), 3000);
        bool add = true;
        for (int i = 0; i < 3000; ++i) { /* each voice alone rounds separately, so allow 1 step */
            if (std::abs((s1[i] + s2[i]) - s3[i]) > 1) add = false;
        }
        expect(add, "two voices together = the sum of each alone (within the 1-step rounding)");
        Wsg m3(prom);
        m3.load_registers(t3);
        m3.set_enabled(true);
        m3.set_voice_mask(1);
        std::vector<int16_t> sm(3000);
        m3.render(sm.data(), 3000);
        expect(sm == s1, "voice mask isolates voice 0");
    }

    // ---------------- Part B: 12C6h = 440 Hz
    {
        Wsg w(prom);
        uint8_t r[32] = {0};
        r[0x10] = 6;
        r[0x11] = 0xC;
        r[0x12] = 2;
        r[0x13] = 1;
        r[0x14] = 0;
        r[0x05] = 0;
        r[0x15] = 15;
        w.load_registers(r);
        w.set_enabled(true);
        std::vector<int16_t> s(Wsg::kRate);
        w.render(s.data(), s.size()); // exactly one second
        double hz_formula = w.frequency_hz(0);
        double best = 0, bestp = -1;
        for (double hz = 400; hz <= 480; hz += 0.25) {
            double pw = goertzel(s, 0, s.size(), hz);
            if (pw > bestp) {
                bestp = pw;
                best = hz;
            }
        }
        int crossings = 0;
        for (size_t i = 1; i < s.size(); ++i)
            if (s[i - 1] < 0 && s[i] >= 0) ++crossings;
        std::printf("  register 12C6h on voice 0: formula %.3f Hz, spectrum peak %.2f Hz, %d rising zero crossings in 1 s\n", hz_formula, best, crossings);
        expect(std::fabs(hz_formula - 440.0) < 4.4, "formula: 0x12C6 * 96000 / 2^20 within 1% of 440 Hz");
        expect(std::fabs(best - 440.0) < 4.4, "measured spectrum peak within 1% of 440 Hz");
        expect(crossings >= 436 && crossings <= 444, "waveform 0 crosses zero upward 440 +/- 1% times in one second");
        if (!out.empty()) write_wav(out + "a440.wav", s);
        // an octave up on voice 1: value 0x258C0 rounds to a multiple of 16 -> about 880 Hz
        Wsg w2(prom);
        uint8_t r2[32] = {0};
        r2[0x16] = 0xC;
        r2[0x17] = 8;
        r2[0x18] = 5;
        r2[0x19] = 2;
        r2[0x0A] = 0;
        r2[0x1A] = 15; // 0x258C0 -> 0x2 5 8 C 0: 0x258C0 = 154816? see value printed
        w2.load_registers(r2);
        w2.set_enabled(true);
        std::vector<int16_t> s2(Wsg::kRate);
        w2.render(s2.data(), s2.size());
        double b2 = 0, bp2 = -1;
        double want = w2.frequency_hz(1);
        for (double hz = want * 0.9; hz <= want * 1.1; hz += 0.25) {
            double pw = goertzel(s2, 0, s2.size(), hz);
            if (pw > bp2) {
                bp2 = pw;
                b2 = hz;
            }
        }
        std::printf("  voice 1 frequency value 0x%X: formula %.2f Hz, spectrum peak %.2f Hz\n", w2.frequency(1), want, b2);
        expect(std::fabs(b2 - want) < want * 0.01, "voice 1 (four-nibble voice): measured pitch equals the formula within 1%");
    }

    // ---------------- Part C: U3, second description
    {
        uint32_t rng = 987654321u;
        auto rnd = [&]() {
            rng = rng * 1664525u + 1013904223u;
            return rng >> 8;
        };
        long compared = 0, bad = 0;
        for (int trial = 0; trial < 40; ++trial) {
            uint8_t r[32];
            for (auto &x : r) x = (uint8_t)(rnd() & 0xFF); // upper nibbles random too: they must be ignored
            Wsg w(prom);
            w.load_registers(r);
            w.set_enabled(true);
            int wave[3], vol[3];
            double fr[3], ph[3] = {0, 0, 0};
            for (int c = 0; c < 3; ++c) {
                wave[c] = r[c * 5 + 5] & 7;
                vol[c] = r[c * 5 + 0x15] & 15;
                double f = (c == 0 ? (r[0x10] & 15) : 0) + (r[c * 5 + 0x11] & 15) * 16.0 + (r[c * 5 + 0x12] & 15) * 256.0 + (r[c * 5 + 0x13] & 15) * 4096.0 +
                           (r[c * 5 + 0x14] & 15) * 65536.0;
                fr[c] = f / 1048576.0; // cycles of the waveform per sample tick
            }
            std::vector<int16_t> got(4000);
            w.render(got.data(), got.size());
            for (size_t i = 0; i < got.size(); ++i) {
                double sum = 0;
                for (int c = 0; c < 3; ++c) {
                    int idx = (int)std::floor((ph[c] - std::floor(ph[c])) * 32.0);
                    sum += ((prom[wave[c] * 32 + idx] & 15) - 8) * (double)vol[c];
                    ph[c] += fr[c];
                }
                int ref = (int)(sum / 384.0 * 32767.0); // truncation toward zero
                ++compared;
                if (std::abs(ref - got[i]) > 1) ++bad; // 1 step allowed for double rounding
            }
        }
        std::printf("  second description vs Wsg: %ld samples compared, %ld differ by more than 1 step\n", compared, bad);
        expect(bad == 0, "U3: Wsg equals the independent floating-point description sample by sample");
    }

    // ---------------- Part D: the start jingle
    {
        Machine m(roms.program);
        Inputs in;
        Wsg w(prom);
        std::vector<std::vector<uint8_t>> regs;
        std::vector<int> enabled;
        std::vector<int16_t> wav;
        const int first = 421, last_max = 900;
        for (int f = 0; f < last_max; ++f) {
            in.set(Control::Coin1, f >= 400 && f < 405);
            in.set(Control::Start1, f >= 420 && f < 424);
            in.apply(m.bus);
            m.run_frame();
            if (f < first) continue;
            regs.emplace_back(m.bus.sound_regs, m.bus.sound_regs + 32);
            enabled.push_back(m.bus.sound_enabled() ? 1 : 0);
        }
        // The jingle is the stretch of music that starts at the first sound. It ends where the siren sweep begins (the music table is used up):
        // the first frame with an audible pitch that is not an entry of the ROM note table.
        const uint8_t *tab = roms.program.data() + 0x3BB9;
        std::set<uint32_t> table_values;
        for (int i = 0; i < 16; ++i)
            for (int sh = 0; sh < 8; ++sh) table_values.insert((uint32_t)tab[i] << sh);
        auto in_table_value = [&](int c, uint32_t fv) { return c == 0 ? table_values.count(fv) != 0 : table_values.count(fv >> 4) != 0; };
        int heard = -1, endf = (int)regs.size();
        for (size_t i = 0; i < regs.size(); ++i) {
            Wsg t(prom);
            t.load_registers(regs[i].data());
            bool any = false, foreign = false;
            for (int c = 0; c < 3; ++c)
                if (t.volume(c) && t.frequency(c) && enabled[i]) {
                    any = true;
                    if (!in_table_value(c, t.frequency(c))) foreign = true;
                }
            if (any && heard < 0) heard = (int)i;
            if (heard >= 0 && foreign) {
                endf = (int)i;
                break;
            }
        }
        expect(heard == 0, "the start jingle makes sound from the first frame after START (frame 421)");
        std::printf("  jingle: sound first heard at frame %d, music ends at frame %d (%d frames, %.2f s)\n", first + heard, first + endf, endf - heard,
                    (endf - heard) / 60.606);
        expect(endf - heard >= 240 && endf - heard <= 280, "the music lasts about 4.2 s (a known length of the Pac-Man start tune: 240-280 frames)");
        {
            Wsg t(prom);
            int steps = 0;
            uint32_t prevf = 0;
            bool sweep = true;
            for (int k = 0; k < 10; ++k) {
                t.load_registers(regs[endf + k].data());
                uint32_t f1 = t.frequency(1);
                if (k > 0 && !(f1 == prevf + 0x200 || f1 + 0x200 == prevf)) sweep = false;
                prevf = f1;
                ++steps;
            }
            expect(sweep, "after the music, the sound is the siren sweep (voice 1 pitch moves in steps of 0x200)");
        }
        // render frame by frame, also each voice alone
        std::vector<int16_t> vwav[3];
        Wsg wv[3] = {Wsg(prom), Wsg(prom), Wsg(prom)};
        for (int c = 0; c < 3; ++c) wv[c].set_voice_mask(1u << c);
        for (int i = heard; i < endf; ++i) {
            w.load_registers(regs[i].data());
            w.set_enabled(enabled[i]);
            size_t at = wav.size();
            wav.resize(at + Wsg::kSamplesPerFrame);
            w.render(wav.data() + at, Wsg::kSamplesPerFrame);
            for (int c = 0; c < 3; ++c) {
                wv[c].load_registers(regs[i].data());
                wv[c].set_enabled(enabled[i]);
                size_t a2 = vwav[c].size();
                vwav[c].resize(a2 + Wsg::kSamplesPerFrame);
                wv[c].render(vwav[c].data() + a2, Wsg::kSamplesPerFrame);
            }
        }
        if (!out.empty()) write_wav(out + "start_jingle.wav", wav);
        bool loud = false;
        for (int16_t v : wav)
            if (std::abs(v) > 2000) loud = true;
        expect(loud && wav.size() == (size_t)(endf - heard) * 1584, "the WAV has the jingle: 1584 samples per frame, clearly audible");
        std::printf("  ROM note table at 3BB9 (16 entries):");
        for (int i = 0; i < 16; ++i) std::printf(" %02X", tab[i]);
        std::printf("\n");
        // collect steady notes per voice: runs of >= 6 frames with the same non-zero frequency and waveform and volume > 0
        int notes = 0, in_table = 0, measured_ok = 0, measured = 0;
        std::set<uint32_t> distinct[3];
        for (int c = 0; c < 3; ++c) {
            int i = heard;
            while (i < endf) {
                Wsg t(prom);
                t.load_registers(regs[i].data());
                uint32_t fv = t.frequency(c);
                int wv_ = t.waveform(c);
                int j = i;
                while (j < endf) {
                    Wsg u(prom);
                    u.load_registers(regs[j].data());
                    if (u.frequency(c) != fv || u.waveform(c) != wv_ || u.volume(c) == 0 || !enabled[j]) break;
                    ++j;
                }
                if (fv != 0 && j - i >= 1) {
                    ++notes;
                    distinct[c].insert(fv);
                    uint32_t unit = c == 0 ? fv : fv;                                       // value as the 20-bit register
                    bool found = table_values.count(unit) || table_values.count(unit >> 4); // voices 1, 2: the register is table*2^k * 16
                    if (c > 0) found = table_values.count(unit >> 4) != 0;
                    if (c == 0) found = table_values.count(unit) != 0;
                    if (found) ++in_table;
                    else std::printf("  note value 0x%X on voice %d (frame %d) is not in the ROM table\n", fv, c, first + i);
                    if (j - i >= 6) {
                        std::vector<int16_t> seg(vwav[c].begin() + (size_t)(i - heard) * 1584, vwav[c].begin() + (size_t)(j - heard) * 1584);
                        double expect_hz = fv * (double)Wsg::kRate / 1048576.0, got = pitch_by_autocorrelation(seg, expect_hz);
                        ++measured;
                        if (got > 0 && std::fabs(got - expect_hz) < expect_hz * 0.01) ++measured_ok;
                        else std::printf("  voice %d frame %d..%d: value 0x%X expects %.2f Hz, measured %.2f Hz\n", c, first + i, first + j, fv, expect_hz, got);
                    }
                }
                i = j > i ? j : i + 1;
            }
        }
        std::printf("  jingle notes (runs of constant pitch): %d, found in the ROM note table: %d; pitch measured in the audio for %d long notes, %d within 1%%\n", notes,
                    in_table, measured, measured_ok);
        for (int c = 0; c < 3; ++c) {
            std::printf("  voice %d distinct pitches (Hz):", c);
            for (uint32_t v : distinct[c]) std::printf(" %.1f", v * 96000.0 / 1048576.0);
            std::printf("\n");
        }
        expect(notes >= 20, "the jingle has at least 20 notes");
        expect(in_table == notes, "every note value is an entry of the ROM note table (shifted by an octave count)");
        expect(measured >= 10 && measured_ok == measured, "the pitch measured in the rendered audio equals the register value's pitch, all long notes within 1%");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    std::printf(failures ? "FAIL test_wsg\n" : "PASS test_wsg\n");
    return failures ? 1 : 0;
}
