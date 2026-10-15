// X12 test of the save slot files in src/app/state.{h,cpp}. Usage: test_slots pacman.zip
// header fields (player, lives, level, score, time) equal the machine's own RAM; pack / unpack round trip; a slot loaded into a BRAND NEW machine gives the same next 300 frames as the
// original; the old quicksave.sav (no header) is read as it is; empty / cut / damaged slot files; file names; the picker lines (24 characters at most).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
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
    } else std::printf("  ok: %s\n", what.c_str());
}
struct Frame {
    uint8_t in0, in1;
};
static std::vector<Frame> make_script(int frames, unsigned seed) {
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
    uint64_t step(const Frame &f) {
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
        h = fnv(h, (const uint8_t *)chip.data(), chip.size() * 2);
        return h;
    }
};
static void write(const std::filesystem::path &p, const std::vector<uint8_t> &b) {
    std::ofstream f(p, std::ios::binary);
    f.write((const char *)b.data(), (std::streamsize)b.size());
}
int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_slots pacman.zip\n");
        return 2;
    }
    setenv("TZ", "UTC", 1);
    tzset();
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    std::vector<Frame> script = make_script(3000, 777);
    std::filesystem::path tmp = "build/x12_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    Rig a(roms);
    for (int f = 0; f < 2000; ++f) a.step(script[f]);
    std::vector<uint8_t> st, file;
    state::capture(a.m, a.wsg, st, 0);
    const int64_t T = 1760423400; // 2025-10-14 0630:00 UTC
    state::slot_pack(a.m, st, T, file);
    state::SlotInfo si, hdr;
    std::vector<uint8_t> back;
    expect(state::slot_unpack(file, back, &si), "a packed slot unpacks");
    hdr = si;
    expect(si.present && !si.legacy, "the slot is not legacy");
    const int pl = a.m.bus.read(0x4E09) ? 2 : 1;
    expect(si.player == pl, "header: player " + std::to_string(si.player));
    expect(si.lives == (int)a.m.bus.read(0x4E14), "header: lives " + std::to_string(si.lives) + " = RAM 4E14");
    expect(si.level == (int)a.m.bus.read(0x4E13) + 1, "header: level " + std::to_string(si.level) + " = RAM 4E13 + 1");
    expect(si.score == state::score_ram(a.m.bus, pl - 1), "header: score " + std::to_string(si.score) + " = the player's score in RAM");
    expect(si.time == T, "header: time");
    expect(back == st, "the state inside is byte for byte the one captured (" + std::to_string(st.size()) + " bytes)");
    expect(file.size() == st.size() + 20, "the header is 20 bytes");
    // a slot loaded into a brand new machine gives the same future
    {
        Rig b(roms);
        uint8_t tag = 9;
        expect(state::restore(b.m, b.wsg, back, &err, &tag), "the unpacked state restores into a new machine");
        bool same = true;
        for (int f = 2000; f < 2300; ++f) {
            if (a.step(script[f]) != b.step(script[f])) {
                same = false;
                break;
            }
        }
        expect(same, "300 frames from the loaded slot are identical to the original's");
    }
    // files
    std::string dir = tmp.string();
    expect(state::slot_path(dir, 1) == (tmp / "quicksave.sav").string(), "slot 1 is quicksave.sav (the old quick save)");
    expect(state::slot_path(dir, 3) == (tmp / "slot3.sav").string() && state::slot_path(dir, 8) == (tmp / "slot8.sav").string(), "slots 2..8 are slotN.sav");
    expect(!state::slot_info(state::slot_path(dir, 3), &si) && !si.present, "no file: the slot is empty");
    expect(state::save_file(state::slot_path(dir, 3), file, &err), "the slot file is written");
    {
        state::SlotInfo fi;
        expect(state::slot_info(state::slot_path(dir, 3), &fi) && fi.present && !fi.legacy && fi.time == T && fi.score == hdr.score && fi.level == hdr.level &&
                   fi.lives == hdr.lives && fi.player == hdr.player,
               "slot_info reads the header from the file");
    }
    {
        std::vector<uint8_t> bytes, inner;
        expect(state::load_file(state::slot_path(dir, 3), bytes, &err) && state::slot_unpack(bytes, inner, nullptr) && inner == st,
               "load_file + unpack gives the state back");
    }
    // the old quicksave.sav: a bare state
    write(tmp / "quicksave.sav", st);
    {
        state::SlotInfo li;
        expect(state::slot_info(state::slot_path(dir, 1), &li) && li.present && li.legacy, "the old quicksave.sav is found as slot 1 and marked as an older save");
        expect(li.time > 1700000000, "its date comes from the file's time");
        std::vector<uint8_t> bytes, inner;
        state::load_file(state::slot_path(dir, 1), bytes, &err);
        expect(state::slot_unpack(bytes, inner, &li) && li.legacy && inner == st, "the old file unpacks to itself");
        Rig c(roms);
        uint8_t tag = 9;
        expect(state::restore(c.m, c.wsg, inner, &err, &tag), "and restores");
    }
    // damaged
    {
        std::vector<uint8_t> cut(file.begin(), file.begin() + 10);
        write(tmp / "slot4.sav", cut);
        state::SlotInfo ci;
        expect(state::slot_info(state::slot_path(dir, 4), &ci), "a cut file is still listed (as an older save)");
        std::vector<uint8_t> inner;
        state::slot_unpack(cut, inner, &ci);
        Rig c(roms);
        bool ok = state::restore(c.m, c.wsg, inner, &err, nullptr);
        expect(!ok, "but it cannot be loaded (refused: " + err + ")");
    }
    {
        std::vector<uint8_t> none;
        write(tmp / "slot5.sav", none);
        state::SlotInfo ei;
        expect(!state::slot_info(state::slot_path(dir, 5), &ei) && !ei.present, "an empty file is an empty slot");
        std::vector<uint8_t> inner;
        expect(!state::slot_unpack(none, inner, nullptr), "unpack of nothing is false");
    }
    {
        std::vector<uint8_t> bad = file;
        bad[4] = 7;
        state::SlotInfo bi;
        std::vector<uint8_t> inner;
        state::slot_unpack(bad, inner, &bi);
        expect(bi.legacy, "an unknown header version is treated as an older save (and the state loader then refuses it)");
    }
    // the picker lines
    {
        state::SlotInfo e;
        expect(state::slot_line(3, e) == "3 EMPTY", "line: 3 EMPTY");
        state::SlotInfo x;
        x.present = true;
        x.time = T;
        x.score = 12340;
        x.level = 2;
        x.lives = 3;
        x.player = 1;
        expect(state::slot_line(3, x) == "3 10-14 0630 L2 12340", "line: " + state::slot_line(3, x));
        x.score = 999999;
        x.level = 255;
        expect(state::slot_line(8, x).size() <= 24 && state::slot_line(8, x) == "8 10-14 0630 L99 999999",
               "line with the biggest numbers fits in 24: " + state::slot_line(8, x));
        state::SlotInfo o;
        o.present = true;
        o.legacy = true;
        o.time = T;
        expect(state::slot_line(1, o) == "1 OLD SAVE 10-14 0630", "line: " + state::slot_line(1, o) + " (older save)");
    }
    std::printf("%d checks, %d failed\n%s\n", checks, failures, failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
