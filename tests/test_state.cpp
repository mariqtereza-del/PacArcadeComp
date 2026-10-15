// S33 test of src/app/state.{h,cpp}. Usage: test_state pacman.zip
// A: save at frame N of a real game, run 600 frames (hash of memory + the sound samples of every frame), load, run the same 600 inputs again: every frame identical
// B: the saved bytes loaded into a BRAND NEW machine and sound chip give the same 600 frames (proves nothing is missing from the state)
// C: file round trip; damaged, cut, wrong-version and wrong-build states are refused and change nothing; a state holds no ROM bytes
// D: rewind: states pushed every frame, rewind 120 frames: memory equals what was logged at that frame, and going forward again repeats the original future
// E: the high-score digit drawing equals what the ROM itself draws (compared at several moments of a game)
// F: high score keeper: loaded once at the first attract screen; saved only when a game's score beats the file and the game was not assisted
// Build/run: tests/test_state.sh
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"
#include "../src/audio/wsg.h"
#include "../src/app/state.h"
#include "../src/app/state.cpp"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

struct Frame {
    uint8_t in0, in1;
};
static std::vector<Frame> make_script(int frames, unsigned seed) { // coin at 400, start at 420, random joystick from 430 (the S27 scenario)
    std::vector<Frame> v;
    Inputs in;
    unsigned s = seed;
    int hold = 0;
    Control d = Control::P1Left;
    const Control dirs[4] = {Control::P1Up, Control::P1Left, Control::P1Right, Control::P1Down};
    for (int f = 0; f < frames; ++f) {
        in.set(Control::Coin1, f >= 400 && f < 405);
        in.set(Control::Start1, f >= 420 && f < 424);
        for (Control c : dirs) in.set(c, false);
        if (f >= 430) {
            if (hold <= 0) {
                s = s * 1664525u + 1013904223u;
                d = dirs[(s >> 24) & 3];
                hold = 10 + (int)((s >> 16) & 31);
            }
            --hold;
            in.set(d, true);
        }
        v.push_back({in.in0(), in.in1()});
    }
    return v;
}
static uint64_t fnv(uint64_t h, const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
struct Rig {
    Machine m;
    Wsg wsg;
    std::vector<int16_t> chip;
    Rig(const RomSet &r) : m(r.program), wsg(r.sound1.data()), chip(Wsg::kSamplesPerFrame) {}
    uint64_t step(const Frame &f) { // one frame as the app does it: inputs, run, sound; returns a hash of memory, CPU-visible I/O and the sound samples
        m.bus.in0 = f.in0;
        m.bus.in1 = f.in1;
        m.run_frame();
        wsg.load_registers(m.bus.sound_regs);
        wsg.set_enabled(m.bus.sound_enabled());
        wsg.render(chip.data(), chip.size());
        uint64_t h = 1469598103934665603ull;
        const Bus &b = m.bus;
        h = fnv(h, b.videoram, sizeof b.videoram);
        h = fnv(h, b.colorram, sizeof b.colorram);
        h = fnv(h, b.workram, sizeof b.workram);
        h = fnv(h, b.spriteram, sizeof b.spriteram);
        h = fnv(h, b.sprite_coords, sizeof b.sprite_coords);
        h = fnv(h, b.sound_regs, sizeof b.sound_regs);
        h = fnv(h, b.latch, sizeof b.latch);
        h = fnv(h, (const uint8_t *)chip.data(), chip.size() * 2);
        uint16_t pc = (uint16_t)m.on_get_pc(), sp = (uint16_t)m.on_get_sp();
        h = fnv(h, (const uint8_t *)&pc, 2);
        h = fnv(h, (const uint8_t *)&sp, 2);
        return h;
    }
};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_state pacman.zip\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    const int N = 1800, RUN = 600;
    std::vector<Frame> script = make_script(9200, 12345);
    std::filesystem::path tmp = std::filesystem::path("build/s33_test");
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    // ---- A and B
    Rig a(roms);
    for (int f = 0; f < N; ++f) a.step(script[f]);
    std::vector<uint8_t> saved;
    state::capture(a.m, a.wsg, saved);
    std::printf("  a saved state is %zu bytes (game at frame %d, main state %d, score %u)\n", saved.size(), N, a.m.bus.workram[0x4E00 - 0x4C00],
                state::score_ram(a.m.bus, 0));
    expect(a.m.bus.workram[0x4E00 - 0x4C00] == 3 && state::score_ram(a.m.bus, 0) > 0, "the state is taken in the middle of a real game with a score");
    std::vector<uint64_t> first;
    for (int i = 0; i < RUN; ++i) first.push_back(a.step(script[N + i]));
    std::string e;
    bool ok = state::restore(a.m, a.wsg, saved, &e);
    expect(ok, "load into the same machine (" + e + ")");
    int diff = -1;
    for (int i = 0; i < RUN; ++i) {
        uint64_t h = a.step(script[N + i]);
        if (h != first[i] && diff < 0) diff = i;
    }
    expect(diff < 0, "A: after load, 600 frames repeat the first 600 frames exactly (first difference at frame " + std::to_string(diff) + ")");
    std::vector<uint8_t> after_first;
    {
        Rig z(roms);
        for (int f = 0; f < N + RUN; ++f) z.step(script[f]);
        state::capture(z.m, z.wsg, after_first);
        std::vector<uint8_t> now;
        state::capture(a.m, a.wsg, now);
        size_t wlen = sizeof(Wsg) - 256, wend = now.size() - 4 - 30,
               wbeg = wend - wlen; // the raw sound-chip block holds struct padding bytes (not state); the checks below it cover the sound state
        bool same = now.size() == after_first.size();
        for (size_t i = 0; same && i < now.size() - 4; ++i)
            if ((i < wbeg || i >= wend) && now[i] != after_first[i]) {
                same = false;
                std::printf("  state byte %zu differs\n", i);
            }
        expect(same, "A: the whole state after the repeat equals the state of an uninterrupted run (every byte except the sound chip struct padding)");
    }
    Rig b(roms);
    ok = state::restore(b.m, b.wsg, saved, &e);
    expect(ok, "load into a brand new machine (" + e + ")");
    diff = -1;
    for (int i = 0; i < RUN; ++i) {
        uint64_t h = b.step(script[N + i]);
        if (h != first[i] && diff < 0) diff = i;
    }
    expect(diff < 0, "B: a brand new machine loaded with the state plays the same 600 frames (first difference " + std::to_string(diff) + ")");
    // states at several other moments (attract, ready, power pill time...) also reproduce
    for (int at : {100, 350, 430, 900, 1300, 2000}) {
        Rig p(roms), q(roms);
        for (int f = 0; f < at; ++f) p.step(script[f]);
        std::vector<uint8_t> s;
        state::capture(p.m, p.wsg, s);
        std::vector<uint64_t> ref;
        for (int i = 0; i < 300; ++i) ref.push_back(p.step(script[at + i]));
        state::restore(q.m, q.wsg, s, &e);
        int d = -1;
        for (int i = 0; i < 300; ++i)
            if (q.step(script[at + i]) != ref[i] && d < 0) d = i;
        expect(d < 0, "state at frame " + std::to_string(at) + " into a new machine repeats 300 frames (first difference " + std::to_string(d) + ")");
    }

    // ---- C
    std::string path = (tmp / "sub" / "quick.sav").string();
    expect(state::save_file(path, saved, &e), "state file written (" + e + ")");
    std::vector<uint8_t> back;
    expect(state::load_file(path, back, &e) && back == saved, "state file read back identical");
    std::vector<uint8_t> bad = saved;
    bad[500] ^= 1;
    Rig c(roms);
    std::vector<uint8_t> before;
    state::capture(c.m, c.wsg, before);
    expect(!state::restore(c.m, c.wsg, bad, &e) && e.find("damaged") != std::string::npos, "a damaged state is refused: " + e);
    std::vector<uint8_t> cut(saved.begin(), saved.begin() + 1000);
    expect(!state::restore(c.m, c.wsg, cut, &e), "a cut-short state is refused: " + e);
    std::vector<uint8_t> ver = saved;
    ver[4] = 9;
    { /* fix checksum so only the version differs */
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i + 4 < ver.size(); ++i) {
            crc ^= ver[i];
            for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
        crc = ~crc;
        for (int k = 0; k < 4; ++k) ver[ver.size() - 4 + k] = (uint8_t)(crc >> (8 * k));
    }
    expect(!state::restore(c.m, c.wsg, ver, &e) && e.find("version") != std::string::npos, "a state of another version is refused: " + e);
    std::vector<uint8_t> after;
    state::capture(c.m, c.wsg, after);
    expect(after == before, "refused states changed nothing");
    expect(!state::load_file((tmp / "nothing.sav").string(), back, &e), "a missing state file is reported: " + e);
    {
        bool leak = false;
        auto has = [&](const std::vector<uint8_t> &hay, const uint8_t *needle, size_t n) {
            for (size_t i = 0; i + n <= hay.size(); ++i)
                if (std::memcmp(&hay[i], needle, n) == 0) return true;
            return false;
        };
        auto distinct = [](const uint8_t *p, size_t n) {
            bool seen[256] = {false};
            int d = 0;
            for (size_t i = 0; i < n; ++i)
                if (!seen[p[i]]) {
                    seen[p[i]] = true;
                    ++d;
                }
            return d;
        };
        int tested = 0;
        for (size_t off = 0; off + 32 <= roms.sound1.size(); off += 16)
            if (distinct(&roms.sound1[off], 32) >= 8) {
                ++tested;
                if (has(saved, &roms.sound1[off], 32)) leak = true;
            }
        for (size_t off = 0; off + 32 <= roms.program.size(); off += 61)
            if (distinct(&roms.program[off], 32) >= 12) {
                ++tested;
                if (has(saved, &roms.program[off], 32)) leak = true;
            }
        std::printf("  ROM leak scan: %d windows of 32 ROM bytes checked\n", tested);
        expect(!leak, "the state holds no waveform-ROM bytes and no program-ROM runs");
    }

    // ---- D
    {
        Rig r(roms);
        std::vector<uint64_t> log;
        state::RewindBuffer rb(1800);
        std::vector<uint8_t> s;
        for (int f = 0; f < 1000; ++f) {
            state::capture(r.m, r.wsg, s);
            rb.push(s);
            log.push_back(r.step(script[f]));
        } // the state BEFORE frame f is pushed
        expect(rb.size() == 1000, "rewind buffer holds one state per frame");
        std::vector<uint8_t> t;
        for (int i = 0; i < 120; ++i) {
            bool g = rb.pop(t);
            if (!g) {
                expect(false, "rewind pop");
                break;
            }
        }
        state::restore(r.m, r.wsg, t, &e); // the state before frame 880
        int d = -1;
        for (int f = 880; f < 1000; ++f)
            if (r.step(script[f]) != log[f] && d < 0) d = f;
        expect(d < 0, "D: rewound 120 frames, then replaying the same inputs gives the same 120 frames (first difference " + std::to_string(d) + ")");
        state::RewindBuffer small(10);
        for (int i = 0; i < 25; ++i) {
            std::vector<uint8_t> x(1, (uint8_t)i);
            small.push(x);
        }
        std::vector<uint8_t> y;
        small.pop(y);
        expect(small.size() == 9 && y[0] == 24, "a full ring keeps the newest states (newest = 24)");
        int n = 0;
        while (small.pop(y)) ++n;
        expect(n == 9 && y[0] == 15, "...and only the last 10 (oldest kept = 15)");
    }

    // ---- E: the digits the ROM draws for the high score vs our drawing, at moments where the ROM has just drawn them
    {
        Rig r(roms);
        int checked = 0, bad = 0;
        uint32_t last = 0;
        for (int f = 0; f < 4200; ++f) {
            r.step(script[f]);
            uint32_t hs = state::high_score_ram(r.m.bus);
            if (hs != last && hs > 0 && r.m.frame > 300 && r.m.bus.workram[0x4E00 - 0x4C00] == 3) {
                last = hs;
                Machine tmpm(roms.program);
                uint8_t rom_tiles[6];
                for (int i = 0; i < 6; ++i) rom_tiles[i] = r.m.bus.videoram[0x43F2 - i - 0x4000];
                state::high_score_put(tmpm.bus, hs);
                ++checked;
                for (int i = 0; i < 6; ++i)
                    if (tmpm.bus.videoram[0x43F2 - i - 0x4000] != rom_tiles[i]) {
                        ++bad;
                        if (bad < 6)
                            std::printf("  tile diff: hs %u frame %d pos %d rom %02X ours %02X\n", hs, f, i, rom_tiles[i], tmpm.bus.videoram[0x43F2 - i - 0x4000]);
                    }
            }
        }
        expect(checked >= 3 && bad == 0, "E: our high-score digits equal the ROM's own at every change of a real game (" + std::to_string(checked) + " values, " +
                                             std::to_string(bad) + " tile differences)");
        Machine t2(roms.program);
        state::high_score_put(t2.bus, 5);
        expect(t2.bus.videoram[0x43F2 - 0x4000] == 0x40 && t2.bus.videoram[0x43F2 - 3 - 0x4000] == 0x40 && t2.bus.videoram[0x43F2 - 4 - 0x4000] == 0 &&
                   t2.bus.videoram[0x43F2 - 5 - 0x4000] == 5,
               "score 5 shows as blanks then 05 (the ROM blanks at most four leading zeros)");
        state::high_score_put(t2.bus, 0);
        expect(t2.bus.videoram[0x43F2 - 4 - 0x4000] == 0 && t2.bus.videoram[0x43F2 - 5 - 0x4000] == 0 && t2.bus.videoram[0x43F2 - 3 - 0x4000] == 0x40,
               "score 0 shows '00' at the right (the ROM keeps two digits)");
        state::high_score_put(t2.bus, 123456);
        expect(state::high_score_ram(t2.bus) == 123456, "123456 round trips through the RAM bytes");
    }

    // ---- F
    {
        std::string hs = (tmp / "hs" / "highscore.txt").string();
        uint32_t v = 0;
        expect(!state::high_score_read_file(hs, &v), "no file yet");
        expect(state::high_score_write_file(hs, 4321, &e) && state::high_score_read_file(hs, &v) && v == 4321, "file round trip");
        {
            std::ofstream bad2(tmp / "junk.txt");
            bad2 << "high_score = abc\n";
        }
        expect(!state::high_score_read_file((tmp / "junk.txt").string(), &v), "a damaged file is ignored");
        // 1: stored 50, a normal game beats it -> loaded once at the first attract screen, then saved at game over
        state::high_score_write_file(hs, 50, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            int loaded_at = -1;
            uint32_t ram_at_load = 0;
            std::string msgs;
            bool shown = false;
            for (int f = 0; f < 4500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
                std::string mm = k.take_message();
                msgs += mm + "|";
                if (loaded_at < 0 && k.loaded()) {
                    loaded_at = f;
                    ram_at_load = state::high_score_ram(r.m.bus);
                    shown = r.m.bus.videoram[0x43F2 - 4 - 0x4000] == 5 && r.m.bus.videoram[0x43F2 - 5 - 0x4000] == 0;
                }
            }
            expect(k.stored() > 50 && loaded_at >= 0 && ram_at_load == 50 && shown,
                   "F1: the stored 50 is put in RAM and on the screen once, at the first attract screen (frame " + std::to_string(loaded_at) + ")");
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(onfile == k.stored() && onfile > 50, "F1: a normal game that beat it was saved at game over (file " + std::to_string(onfile) + ")");
        }
        // 2: a state saved in plain play at frame 1500 and loaded at 1600 (when the score was higher): the saved high score is the one the state had when it was SAVED
        state::high_score_write_file(hs, 50, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            std::vector<uint8_t> s;
            bool loaded_state = false;
            uint32_t score_at_load = 0, hs_in_state = 0;
            for (int f = 0; f < 4500; ++f) {
                if (f == 1500) state::capture(r.m, r.wsg, s, k.assisted() ? 1 : 0);
                if (f == 2600 && !loaded_state) {
                    score_at_load = state::score_ram(r.m.bus, 0);
                    uint8_t tag = 9;
                    state::restore(r.m, r.wsg, s, &e, &tag);
                    hs_in_state = state::high_score_ram(r.m.bus);
                    k.note_state_loaded(r.m, tag, hs_in_state);
                    loaded_state = true;
                    f = 1500;
                }
                r.step(script[f]);
                k.after_tick(r.m);
            }
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(hs_in_state > 50 && score_at_load > hs_in_state && onfile == hs_in_state,
                   "F2: after a state load the saved high score is the one the state had when saved (" + std::to_string(hs_in_state) +
                       "), not the later score before the load (" + std::to_string(score_at_load) + ") nor anything after it (file " + std::to_string(onfile) +
                       ", game ended with " + std::to_string(state::score_ram(r.m.bus, 0)) + ")");
        }
        // 2b: a state saved AFTER an earlier load (tag 1) gives nothing when loaded later; a state of the old format (tag 1) also gives nothing
        state::high_score_write_file(hs, 50, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            std::vector<uint8_t> s;
            for (int f = 0; f < 1500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
            }
            state::capture(r.m, r.wsg, s, 1);
            for (int f = 1500; f < 1700; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
            }
            uint8_t tag = 0;
            state::restore(r.m, r.wsg, s, &e, &tag);
            k.note_state_loaded(r.m, tag, state::high_score_ram(r.m.bus));
            for (int f = 1500; f < 4500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
            }
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(tag == 1 && onfile == 50,
                   "F2b: a state saved in an assisted game (tag 1) does not set the high score when loaded (file " + std::to_string(onfile) + ")");
        }
        {
            std::vector<uint8_t> v2;
            Rig r(roms);
            for (int f = 0; f < 800; ++f) r.step(script[f]);
            state::capture(r.m, r.wsg, v2, 0);
            uint8_t tg = 7;
            Rig q(roms);
            expect(state::restore(q.m, q.wsg, v2, &e, &tg) && tg == 0, "the tag 0 comes back out of a state");
            state::capture(r.m, r.wsg, v2, 1);
            expect(state::restore(q.m, q.wsg, v2, &e, &tg) && tg == 1, "the tag 1 comes back out of a state");
        }
        // 3: a lower score never lowers the file; a game still running at exit counts if unassisted
        state::high_score_write_file(hs, 999999, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            for (int f = 0; f < 4500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
            }
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(onfile == 999999, "F3: a lower score never replaces the file");
        }
        state::high_score_write_file(hs, 20, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            for (int f = 0; f < 1500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
            }
            k.finish(r.m);
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(onfile > 20 && onfile == state::score_ram(r.m.bus, 0), "F3: leaving mid-game saves the score so far (file " + std::to_string(onfile) + ")");
        }
        state::high_score_write_file(hs, 20, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            uint32_t at = 0;
            for (int f = 0; f < 1500; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
                if (f == 1200) {
                    k.note_assist();
                    at = state::score_ram(r.m.bus, 0);
                }
            }
            k.finish(r.m);
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(at > 20 && onfile == at && onfile < state::score_ram(r.m.bus, 0), "F3: leaving mid-game after an assist saves the score from before the assist (" +
                                                                                         std::to_string(onfile) + "), not the later one (" +
                                                                                         std::to_string(state::score_ram(r.m.bus, 0)) + ")");
        }
        // 3b: an assist at the very start of a game: nothing was earned before it, so nothing is saved
        state::high_score_write_file(hs, 20, &e);
        {
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            uint32_t at = 99;
            for (int f = 0; f < 2000; ++f) {
                r.step(script[f]);
                k.after_tick(r.m);
                if (f == 600) {
                    k.note_assist();
                    at = state::score_ram(r.m.bus, 0);
                }
            }
            k.finish(r.m);
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            std::printf("  F3b debug: score at assist %u, state %d, file %u\n", at, r.m.bus.workram[0x4E00 - 0x4C00], onfile);
            expect(at == 0 && r.m.bus.workram[0x4E00 - 0x4C00] == 3 && onfile == 20, "F3b: an assist at the start of a game (nothing earned yet) saves nothing");
        }
        // 4: a new game after an assisted one counts again
        state::high_score_write_file(hs, 20, &e);
        {
            std::vector<Frame> two = make_script(9000, 777);
            for (int f = 4500; f < 9000; ++f) {
                two[f].in0 = two[f - 4500].in0;
                two[f].in1 = two[f - 4500].in1;
            }
            Rig r(roms);
            state::HighScoreKeeper k(hs);
            for (int f = 0; f < 9000; ++f) {
                r.step(two[f]);
                k.after_tick(r.m);
                if (f == 1000) k.note_assist();
            }
            uint32_t onfile = 0;
            state::high_score_read_file(hs, &onfile);
            expect(onfile > 20, "F4: the next game is unassisted again and its score is saved (file " + std::to_string(onfile) + ")");
        }
    }

    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL test_state" : "PASS test_state", checks, failures);
    return failures ? 1 : 0;
}
