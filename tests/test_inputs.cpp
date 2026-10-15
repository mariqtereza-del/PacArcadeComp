// S23 test of src/board/inputs.{h,cpp}. Usage: test_inputs pacman.zip [out_prefix]
// Part A: every control sets exactly its own bit (active low) in the right port; release restores idle; the values equal the masks the
//         Python prototype used (tools/analysis/oracle_ram.py inputs()).
// Part B: the bytes reach the CPU through the bus at 5000, 5040-507F, 5080-50BF.
// Part C: DIP byte encode/decode for all 256 values, and the factory default 0xC9.
// Part D: scripted game on the C++ machine: coin at frames 400-404, start 1 at 420-423 (the prototype's scenario S1). The screen text is read from
//         video RAM (tile dump): CREDIT with digit 0 before the coin, digit 1 after it; READY! after start; game state 4E00 = 3.
// Part E: the DIP table in hardware_facts.md section 6 confirmed by running the ROM: for each setting the ROM's own derived values (4E6B-4E75) are read.
// Part F: coinage by playing: 2 coins per credit needs two coins; free play starts with no coin.
// Build: g++ -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_inputs.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp miniz*.o
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
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

// ---- screen text from video RAM (same addressing as the prototype's tile_offset; screen column sx 0..27, row sy 0..35)
static int tile_offset(int col, int row) {
    int r = row + 2, c = col - 2;
    return (c & 0x20) ? r + ((c & 0x1f) << 5) : c + (r << 5);
}
static uint8_t tile(const Machine &m, int sx, int sy) { return m.bus.videoram[tile_offset(sy, 27 - sx)]; }
static std::string row_text(const Machine &m, int sy) {
    std::string s;
    for (int sx = 0; sx < 28; ++sx) {
        uint8_t t = tile(m, sx, sy);
        s += (t >= 0x30 && t <= 0x5A) ? (char)t : (t == 0x40 || t == 0 ? ' ' : '.');
    }
    return s;
}
static int find_text(const Machine &m, const char *w, int *col = nullptr) {
    for (int sy = 0; sy < 36; ++sy) {
        size_t p = row_text(m, sy).find(w);
        if (p != std::string::npos) {
            if (col) *col = (int)p;
            return sy;
        }
    }
    return -1;
}
static int credit_digit(const Machine &m) { // the digit after CREDIT on the bottom row (-1 = none)
    int col;
    int sy = find_text(m, "CREDIT", &col);
    if (sy < 0) return -2;
    for (int sx = col + 6; sx < 28; ++sx) {
        uint8_t t = tile(m, sx, sy);
        if (t <= 9) return t;
        if (t >= 0x30 && t <= 0x39) return t - 0x30;
    }
    return -1;
}
static void save_ppm_tiles(const std::string &fn, const Machine &m) { // text dump of the whole screen, for the evidence file
    FILE *o = std::fopen(fn.c_str(), "wb");
    if (!o) return;
    for (int sy = 0; sy < 36; ++sy) std::fprintf(o, "%2d |%s|\n", sy, row_text(m, sy).c_str());
    std::fclose(o);
}

struct Script {
    std::vector<std::pair<int, int>> coins, start1, start2;
}; // [from, to) frame ranges
static bool in_range(const std::vector<std::pair<int, int>> &v, int f) {
    for (auto &r : v)
        if (f >= r.first && f < r.second) return true;
    return false;
}
static void drive(Inputs &in, const Script &s, int f) {
    in.set(Control::Coin1, in_range(s.coins, f));
    in.set(Control::Start1, in_range(s.start1, f));
    in.set(Control::Start2, in_range(s.start2, f));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_inputs pacman.zip [out_prefix]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }

    // ---------------- Part A: bit wiring
    {
        struct W {
            Control c;
            int port, bit;
            const char *name;
        };
        const W w[] = {
            {Control::P1Up, 0, 0, "P1Up"},
            {Control::P1Left, 0, 1, "P1Left"},
            {Control::P1Right, 0, 2, "P1Right"},
            {Control::P1Down, 0, 3, "P1Down"},
            {Control::RackTest, 0, 4, "RackTest"},
            {Control::Coin1, 0, 5, "Coin1"},
            {Control::Coin2, 0, 6, "Coin2"},
            {Control::ServiceCoin, 0, 7, "ServiceCoin"},
            {Control::P2Up, 1, 0, "P2Up"},
            {Control::P2Left, 1, 1, "P2Left"},
            {Control::P2Right, 1, 2, "P2Right"},
            {Control::P2Down, 1, 3, "P2Down"},
            {Control::ServiceMode, 1, 4, "ServiceMode"},
            {Control::Start1, 1, 5, "Start1"},
            {Control::Start2, 1, 6, "Start2"},
            {Control::Cocktail, 1, 7, "Cocktail"},
        };
        Inputs in;
        expect(in.in0() == 0xFF && in.in1() == 0xFF, "idle: IN0 = IN1 = 0xFF");
        for (const W &x : w) {
            Inputs a;
            a.set(x.c, true);
            uint8_t want0 = x.port == 0 ? (uint8_t) ~(1u << x.bit) : 0xFF, want1 = x.port == 1 ? (uint8_t) ~(1u << x.bit) : 0xFF;
            expect(a.in0() == want0 && a.in1() == want1,
                   std::string(x.name) + " pressed clears exactly bit " + std::to_string(x.bit) + " of IN" + std::to_string(x.port));
            expect(a.pressed(x.c), std::string(x.name) + " reports pressed");
            a.set(x.c, false);
            expect(a.in0() == 0xFF && a.in1() == 0xFF && !a.pressed(x.c), std::string(x.name) + " released returns to idle");
        }
        {
            Inputs a;
            a.set(Control::Coin1, true);
            expect(a.in0() == 0xDF, "coin 1 = 0xDF (prototype mask)");
        }
        {
            Inputs a;
            a.set(Control::Start1, true);
            expect(a.in1() == 0xDF, "start 1 = 0xDF (prototype mask)");
        }
        {
            Inputs a;
            a.set(Control::Start2, true);
            expect(a.in1() == 0xBF, "start 2 = 0xBF (prototype mask)");
        }
        {
            Inputs a;
            a.set(Control::RackTest, true);
            expect(a.in0() == 0xEF, "rack test = 0xEF (prototype mask)");
        }
        {
            Inputs a;
            a.set(Control::P1Up, true);
            a.set(Control::P1Left, true);
            expect(a.in0() == 0xFC, "two directions pressed together are both 0");
        }
        {
            Inputs a;
            a.set(Control::Cocktail, true);
            expect(a.in1() == 0x7F, "cocktail cabinet = IN1 0x7F (prototype value)");
            a.set(Control::Start1, true);
            a.release_all();
            expect(a.in1() == 0x7F && a.in0() == 0xFF, "release_all keeps the cabinet switch");
        }
    }

    // ---------------- Part B: through the bus
    {
        Machine m(roms.program);
        Inputs in;
        in.set(Control::Coin1, true);
        in.set(Control::Start2, true);
        in.apply(m.bus);
        expect(m.bus.read(0x5000) == 0xDF, "bus read 5000 = IN0 with coin 1");
        bool all = true;
        for (int a = 0x5040; a < 0x5080; ++a)
            if (m.bus.read(a) != 0xBF) all = false;
        expect(all, "bus read 5040-507F = IN1 with start 2 (every address)");
        all = true;
        for (int a = 0x5080; a < 0x50C0; ++a)
            if (m.bus.read(a) != 0xC9) all = false;
        expect(all, "bus read 5080-50BF = DIP 1 = 0xC9 (every address)");
    }

    // ---------------- Part C: DIP bytes
    {
        expect(DipSettings().to_byte() == 0xC9, "default DIP settings give 0xC9");
        bool rt = true;
        for (int v = 0; v < 256; ++v)
            if (DipSettings::from_byte((uint8_t)v).to_byte() != v) rt = false;
        expect(rt, "all 256 DIP bytes survive decode then encode");
        DipSettings d = DipSettings::from_byte(0xC9);
        expect(d.coinage == DipSettings::OneCoinOneCredit && d.lives == DipSettings::Three && d.bonus == DipSettings::At10000 && d.normal_difficulty &&
                   d.normal_ghost_names,
               "0xC9 decodes to 1 coin 1 credit, 3 lives, bonus 10,000, normal, normal names");
        DipSettings e;
        e.coinage = DipSettings::FreePlay;
        e.lives = DipSettings::Five;
        e.bonus = DipSettings::NoBonus;
        e.normal_difficulty = false;
        e.normal_ghost_names = false;
        expect(e.to_byte() == 0x30 + 0x0C, "free play + 5 lives + no bonus + hard + alternate names = 0x3C");
        Inputs in;
        in.set_dip(e);
        Machine m(roms.program);
        in.apply(m.bus);
        expect(m.bus.read(0x5080) == 0x3C, "DIP reaches the bus");
    }

    std::string out = argc > 2 ? argv[2] : "";

    // ---------------- Part D: scripted game (prototype scenario S1)
    {
        Machine m(roms.program);
        Inputs in;
        Script s;
        s.coins = {{400, 405}};
        s.start1 = {{420, 424}};
        int digit_before = -9, digit_after = -9, credits_after = -1, ready_frame = -1, state_at_ready = -1, state_before_start = -1;
        int col = -1;
        std::string ready_row;
        bool credit_word = false;
        for (int f = 0; f < 560; ++f) {
            drive(in, s, f);
            in.apply(m.bus);
            m.run_frame();
            if (f == 399) {
                digit_before = credit_digit(m);
                credit_word = find_text(m, "CREDIT") >= 0;
                state_before_start = m.bus.read(0x4E00);
            }
            if (f == 410) {
                digit_after = credit_digit(m);
                credits_after = m.bus.read(0x4E6E);
            }
            if (ready_frame < 0 && f >= 421 && find_text(m, "READY", &col) >= 0) {
                ready_frame = f;
                state_at_ready = m.bus.read(0x4E00);
                int sy = find_text(m, "READY", &col);
                ready_row = row_text(m, sy);
                uint8_t bang = tile(m, col + 5, sy);
                std::printf("  READY tile after Y: code 0x%02X (row %d, col %d)\n", bang, sy, col);
                expect(bang != 0x40 && bang != 0 && !(bang >= 0x30 && bang <= 0x5A), "a mark tile (the '!') follows READY");
                if (!out.empty()) save_ppm_tiles(out + "ready_text_dump.txt", m);
            }
        }
        std::printf("  attract before coin: CREDIT word %s, digit %d; after coin: digit %d, 4E6E=%d; READY at frame %d, 4E00=%d\n", credit_word ? "yes" : "no",
                    digit_before, digit_after, credits_after, ready_frame, state_at_ready);
        expect(credit_word, "the word CREDIT is on screen in attract mode");
        expect(digit_before == 0, "CREDIT shows 0 before the coin");
        expect(state_before_start == 1, "main state 4E00 = 1 (attract) before the coin");
        expect(digit_after == 1 && credits_after == 1, "CREDIT shows 1 after the coin (and credits 4E6E = 1)");
        expect(ready_frame >= 421 && ready_frame <= 440, "READY appears right after start (prototype: frame 421)");
        expect(ready_frame >= 0, "READY! text found after start");
        expect(m.bus.read(0x4E00) == 3 && m.bus.read(0x4E6E) == 0, "game running (4E00 = 3), credit used (4E6E = 0)");
        // no start without a coin: same script, no coin
        Machine m2(roms.program);
        Inputs in2;
        Script s2;
        s2.start1 = {{420, 424}};
        bool seen = false;
        for (int f = 0; f < 560; ++f) {
            drive(in2, s2, f);
            in2.apply(m2.bus);
            m2.run_frame();
            if (find_text(m2, "READY") >= 0) seen = true;
        }
        expect(!seen && m2.bus.read(0x4E00) == 1, "pressing start without a coin does not start the game");
    }

    // ---------------- Part E: DIP table confirmed by the ROM's own derived values
    {
        struct Case {
            const char *label;
            uint8_t dip;
            int cpc, cred_per_coin, lives, bonus, diff_lo, names;
        };
        const Case cs[] = {
            {"C9 default", 0xC9, 1, 1, 3, 0x10, 0x68, 0},
            {"CB 2 coins 1 credit", 0xCB, 2, 1, 3, 0x10, 0x68, 0},
            {"CA 1 coin 2 credits", 0xCA, 1, 2, 3, 0x10, 0x68, 0},
            {"C8 free play", 0xC8, 0, 0, 3, 0x10, 0x68, 0},
            {"C1 one life", 0xC1, 1, 1, 1, 0x10, 0x68, 0},
            {"C5 two lives", 0xC5, 1, 1, 2, 0x10, 0x68, 0},
            {"CD five lives", 0xCD, 1, 1, 5, 0x10, 0x68, 0},
            {"D9 bonus 15000", 0xD9, 1, 1, 3, 0x15, 0x68, 0},
            {"E9 bonus 20000", 0xE9, 1, 1, 3, 0x20, 0x68, 0},
            {"F9 no bonus", 0xF9, 1, 1, 3, 0xFF, 0x68, 0},
            {"89 hard", 0x89, 1, 1, 3, 0x10, 0x7D, 0},
            {"49 alternate names", 0x49, 1, 1, 3, 0x10, 0x68, 1},
        };
        for (const Case &c : cs) {
            Machine m(roms.program);
            Inputs in;
            in.set_dip(DipSettings::from_byte(c.dip));
            for (int f = 0; f < 300; ++f) {
                in.apply(m.bus);
                m.run_frame();
            }
            auto r = [&](int a) { return (int)m.bus.read(a); };
            bool ok = r(0x4E6B) == c.cpc && r(0x4E6D) == c.cred_per_coin && r(0x4E6F) == c.lives && r(0x4E71) == c.bonus && r(0x4E73) == c.diff_lo &&
                      r(0x4E75) == c.names && r(0x4E72) == 0;
            std::printf("  DIP %-22s ROM: coins/credit %d, credits/coin %d, lives %d, bonus 0x%02X, diff ptr 0x%02X, alt names %d, cocktail %d\n", c.label, r(0x4E6B),
                        r(0x4E6D), r(0x4E6F), r(0x4E71), r(0x4E73), r(0x4E75), r(0x4E72));
            expect(ok, std::string("DIP ") + c.label + ": the ROM derives the settings the table says");
        }
        Machine m(roms.program);
        Inputs in;
        in.set(Control::Cocktail, true);
        for (int f = 0; f < 300; ++f) {
            in.apply(m.bus);
            m.run_frame();
        }
        expect(m.bus.read(0x4E72) == 1, "cocktail switch: the ROM sets 4E72 = 1");
    }

    // ---------------- Part F: coinage by playing
    {
        Machine m(roms.program);
        Inputs in;
        in.set_dip(DipSettings::from_byte(0xCB));
        Script s;
        s.coins = {{400, 405}, {440, 445}};
        int c1 = -1, c2 = -1;
        for (int f = 0; f < 460; ++f) {
            drive(in, s, f);
            in.apply(m.bus);
            m.run_frame();
            if (f == 420) c1 = credit_digit(m);
            if (f == 459) c2 = credit_digit(m);
        }
        std::printf("  2 coins per credit: after coin 1 CREDIT %d, after coin 2 CREDIT %d\n", c1, c2);
        expect(c1 == 0 && c2 == 1, "2 coins = 1 credit: one coin shows 0, two coins show 1");
        Machine m2(roms.program);
        Inputs i2;
        i2.set_dip(DipSettings::from_byte(0xCA));
        Script s2;
        s2.coins = {{400, 405}};
        int d = -1;
        for (int f = 0; f < 420; ++f) {
            drive(i2, s2, f);
            i2.apply(m2.bus);
            m2.run_frame();
        }
        d = credit_digit(m2);
        expect(d == 2, "1 coin = 2 credits: one coin shows 2");
        Machine m3(roms.program);
        Inputs i3;
        i3.set_dip(DipSettings::from_byte(0xC8));
        Script s3;
        s3.start1 = {{420, 424}};
        bool ready = false;
        for (int f = 0; f < 480; ++f) {
            drive(i3, s3, f);
            i3.apply(m3.bus);
            m3.run_frame();
            if (find_text(m3, "READY") >= 0) ready = true;
        }
        expect(ready, "free play: start without a coin shows READY");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    std::printf(failures ? "FAIL test_inputs\n" : "PASS test_inputs\n");
    return failures ? 1 : 0;
}
