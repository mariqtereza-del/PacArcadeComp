// S17 test of src/board/bus.{h,cpp}. Build: g++ -std=c++17 -Wall -Wextra tests/test_bus.cpp src/board/bus.cpp -o test_bus
// Part A: all 65,536 addresses, read and write, are checked against an independent expansion of MAME's pacman_map()
//         (the entries below are copied from pacman.cpp lines 1049-1064, in the order MAME lists them).
// Part B: behaviour per region, each tagged with its row in docs/notes/hardware_facts.md (section 2 map, 3 latch, 6 inputs).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/bus.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
static std::string hx(unsigned v) {
    char b[16];
    std::snprintf(b, sizeof b, "%04X", v);
    return b;
}

struct MameEntry {
    uint16_t start, end, mirror;
    Region region;
    bool is_write;
};
// pacman_map(), written as (start, end, mirror, what it does for reads or writes). Later entries win, as in MAME.
static const MameEntry MAP[] = {
    {0x0000, 0x3fff, 0x8000, Region::Rom, false},         {0x4000, 0x43ff, 0xa000, Region::VideoRam, false},
    {0x4000, 0x43ff, 0xa000, Region::VideoRam, true},     {0x4400, 0x47ff, 0xa000, Region::ColorRam, false},
    {0x4400, 0x47ff, 0xa000, Region::ColorRam, true},     {0x4800, 0x4bff, 0xa000, Region::Nop, false}, // read nop; write nopw -> Region::None
    {0x4c00, 0x4fef, 0xa000, Region::WorkRam, false},     {0x4c00, 0x4fef, 0xa000, Region::WorkRam, true},
    {0x4ff0, 0x4fff, 0xa000, Region::SpriteRam, false},   {0x4ff0, 0x4fff, 0xa000, Region::SpriteRam, true},
    {0x5000, 0x5007, 0xaf38, Region::Latch, true},        {0x5040, 0x505f, 0xaf00, Region::Sound, true},
    {0x5060, 0x506f, 0xaf00, Region::SpriteCoords, true}, {0x50c0, 0x50c0, 0xaf3f, Region::Watchdog, true},
    {0x5000, 0x5000, 0xaf3f, Region::In0, false},         {0x5040, 0x5040, 0xaf3f, Region::In1, false},
    {0x5080, 0x5080, 0xaf3f, Region::Dsw1, false},        {0x50c0, 0x50c0, 0xaf3f, Region::Dsw2, false},
};
// (5070-507F and 5080 are nopw entries: Region::None, which is also what an address with no write entry gives.)

static Decoded mame_expect(uint32_t a, bool write) {
    Decoded r = {Region::None, 0};
    if (!write) r = {Region::None, 0}; // no read entry: open bus (not used by the ROM; the bus returns 0xFF)
    for (const MameEntry &e : MAP) {
        if (e.is_write != write) continue;
        if (!write && e.region == Region::Nop) { /* read entry */
        }
        uint32_t base = a & ~(uint32_t)e.mirror & 0xFFFF;
        if (base >= e.start && base <= e.end) {
            uint32_t off = base - e.start;
            r = {e.region, (uint16_t)off};
        }
    }
    return r;
}

int main() {
    // ---------------- Part A: every address against the MAME map
    int bad_r = 0, bad_w = 0;
    for (uint32_t a = 0; a < 0x10000; ++a) {
        Decoded er = mame_expect(a, false), ew = mame_expect(a, true);
        Decoded gr = Bus::decode_read((uint16_t)a), gw = Bus::decode_write((uint16_t)a);
        // reads and writes with offsets that do not matter for single-address entries
        bool single_r = er.region == Region::In0 || er.region == Region::In1 || er.region == Region::Dsw1 || er.region == Region::Dsw2;
        bool single_w = ew.region == Region::Watchdog;
        if (er.region != gr.region || (!single_r && er.offset != gr.offset)) {
            if (++bad_r <= 5) std::printf("  read %s differs\n", hx(a).c_str());
        }
        if (ew.region != gw.region || (!single_w && ew.offset != gw.offset)) {
            if (++bad_w <= 5) std::printf("  write %s differs\n", hx(a).c_str());
        }
    }
    std::printf("Part A: 65536 read decodes and 65536 write decodes compared with MAME's map: %d read and %d write differences\n", bad_r, bad_w);
    expect(bad_r == 0, "read decode equals MAME map for all addresses");
    expect(bad_w == 0, "write decode equals MAME map for all addresses");

    // ---------------- Part B: behaviour, one region at a time
    Bus b;
    std::vector<uint8_t> rom(0x4000);
    for (size_t i = 0; i < rom.size(); ++i) rom[i] = (uint8_t)(i * 7 + (i >> 8));
    expect(b.load_program(rom) && !b.load_program(std::vector<uint8_t>(100)), "load_program accepts exactly 16384 bytes");

    // [section 2 row 0000-3FFF] ROM read-only, repeated at 8000-BFFF
    expect(b.read(0x0000) == rom[0] && b.read(0x3FFF) == rom[0x3FFF] && b.read(0x1234) == rom[0x1234], "ROM reads at 0000, 3FFF, 1234");
    expect(b.read(0x8000) == rom[0] && b.read(0xBFFF) == rom[0x3FFF] && b.read(0x9234) == rom[0x1234], "ROM copy at 8000-BFFF");
    b.write(0x1234, 0xEE);
    b.write(0x9234, 0xEE);
    expect(b.read(0x1234) == rom[0x1234] && b.read(0x9234) == rom[0x1234], "writes to ROM are ignored");

    // [section 2 row 4000-43FF] video RAM, mirrors (MAME mirror 0xA000)
    b.write(0x4000, 0x11);
    b.write(0x43FF, 0x22);
    b.write(0x4123, 0x33);
    expect(b.read(0x4000) == 0x11 && b.read(0x43FF) == 0x22 && b.read(0x4123) == 0x33, "video RAM read back");
    expect(b.read(0x6000) == 0x11 && b.read(0xC000) == 0x11 && b.read(0xE000) == 0x11 && b.read(0xE3FF) == 0x22, "video RAM mirrors 6000, C000, E000");
    b.write(0xE123, 0x44);
    expect(b.read(0x4123) == 0x44 && b.videoram[0x123] == 0x44, "write through a mirror lands in the same cell");
    // The note says "repeated at A000"; MAME's 0xA000 is a mask of address bits, giving 6000, C000 and E000 (A000-BFFF is ROM): recorded in S17 evidence.
    expect(b.read(0xA000) == rom[0x2000], "A000 is the ROM copy, not video RAM (note's 'A000' means mirror bits)");

    // [section 2 row 4400-47FF] colour RAM
    b.write(0x4400, 0x5A);
    b.write(0x47FF, 0xFF);
    expect(b.read(0x4400) == 0x5A && b.read(0x47FF) == 0xFF && b.colorram[0x3FF] == 0xFF, "colour RAM keeps all 8 bits (masking & 0x1F is for the renderer)");
    expect(b.read(0x6400) == 0x5A && b.read(0xE7FF) == 0xFF, "colour RAM mirrors");
    expect(b.read(0x4000) == 0x11, "colour RAM write did not touch video RAM");

    // [section 2 row 4800-4BFF] not RAM: reads 0xBF, writes ignored
    b.write(0x4800, 0x12);
    b.write(0x4BFF, 0x34);
    expect(b.read(0x4800) == 0xBF && b.read(0x4BFF) == 0xBF && b.read(0x4A00) == 0xBF && b.read(0xCA00) == 0xBF, "4800-4BFF reads 0xBF after writes");
    expect(b.read(0x4C00) == 0 && b.videoram[0] == 0x11, "ignored writes did not land elsewhere");

    // [section 2 row 4C00-4FEF] work RAM; [row 4FF0-4FFF] sprite RAM
    b.write(0x4C00, 0xA1);
    b.write(0x4FEF, 0xA2);
    b.write(0x4FF0, 0xB1);
    b.write(0x4FFF, 0xB2);
    expect(b.read(0x4C00) == 0xA1 && b.read(0x4FEF) == 0xA2 && b.read(0x4FF0) == 0xB1 && b.read(0x4FFF) == 0xB2, "work RAM and sprite RAM boundaries");
    expect(b.workram[0] == 0xA1 && b.workram[0x3EF] == 0xA2 && b.spriteram[0] == 0xB1 && b.spriteram[15] == 0xB2, "stored in the right arrays");
    expect(b.read(0x6C00) == 0xA1 && b.read(0xCFEF) == 0xA2 && b.read(0xEFF0) == 0xB1, "work/sprite RAM mirrors");

    // [section 3] 5000-5007 latch, only bit 0 counts; mirror 0xAF38
    for (int i = 0; i < 8; ++i) b.write(0x5000 + i, (uint8_t)(0xFE | (i & 1))); // bit 0 = 0 for even, 1 for odd
    expect(b.latch[0] == 0 && b.latch[1] == 1 && b.latch[2] == 0 && b.latch[3] == 1 && b.latch[7] == 1, "latch stores data bit 0 only");
    b.write(0x5000, 1);
    expect(b.irq_enabled(), "5000 bit 0 = interrupt enable");
    b.write(0x5001, 1);
    expect(b.sound_enabled(), "5001 = sound enable");
    b.write(0x5003, 1);
    expect(b.flip_screen(), "5003 = flip screen");
    b.write(0x5007, 1);
    expect(b.coin_counter(), "5007 = coin counter");
    b.write(0x5008, 0);
    b.write(0x5003, 0);
    b.write(0x5039, 1); // 5008 -> latch 0, 5039 -> latch 1
    expect(!b.irq_enabled() && b.latch[1] == 1 && !b.flip_screen(), "latch mirror: address bits 3-5 ignored (5008 = 5000, 5039 = 5001)");
    b.write(0x7000 + 0, 1);
    b.write(0xD001, 0);
    expect(b.irq_enabled() && !b.sound_enabled(), "latch mirrors at 7000 and D001");

    // [section 2 rows 5040-505F, 5060-506F] sound registers and sprite coordinates (write only)
    b.write(0x5040, 0x0A);
    b.write(0x505F, 0x0B);
    b.write(0x5060, 0xC1);
    b.write(0x506F, 0xC2);
    b.write(0x5155, 0x07);
    expect(b.sound_regs[0] == 0x0A && b.sound_regs[0x1F] == 0x0B && b.sprite_coords[0] == 0xC1 && b.sprite_coords[15] == 0xC2, "sound / sprite-coordinate boundaries");
    expect(b.sound_regs[0x15] == 0x07, "5155 mirrors to 5055 (address bits 8-11 ignored)");
    // [section 2 rows 5070-507F, 5080] writes ignored
    uint8_t before_s[0x20], before_c[0x10];
    std::memcpy(before_s, b.sound_regs, 0x20);
    std::memcpy(before_c, b.sprite_coords, 0x10);
    for (int a = 0x5070; a <= 0x50BF; ++a) b.write((uint16_t)a, 0x99);
    expect(!std::memcmp(before_s, b.sound_regs, 0x20) && !std::memcmp(before_c, b.sprite_coords, 0x10) && b.latch[0] == 1, "5070-50BF writes change nothing");
    // [row 50C0] watchdog
    uint32_t w0 = b.watchdog_writes;
    b.write(0x50C0, 0);
    b.write(0x50FF, 0);
    b.write(0x58C0, 0);
    expect(b.watchdog_writes == w0 + 3, "50C0-50FF (and mirrors) count as watchdog writes");

    // [section 6 + section 2 read rows] input ports, mirrored over each 64-byte block
    b.in0 = 0x12;
    b.in1 = 0x34;
    b.dsw1 = 0xC9;
    b.dsw2 = 0x56;
    expect(b.read(0x5000) == 0x12 && b.read(0x503F) == 0x12 && b.read(0x5001) == 0x12, "5000-503F reads IN0");
    expect(b.read(0x5040) == 0x34 && b.read(0x507F) == 0x34 && b.read(0x5060) == 0x34, "5040-507F reads IN1 (also over the write-only sprite registers)");
    expect(b.read(0x5080) == 0xC9 && b.read(0x50BF) == 0xC9, "5080-50BF reads DIP switch 1");
    expect(b.read(0x50C0) == 0x56 && b.read(0x50FF) == 0x56, "50C0-50FF reads DIP switch 2");
    expect(b.read(0x5340) == 0x34 && b.read(0xD080) == 0xC9 && b.read(0x7000) == 0x12, "input mirrors at 5340, D080, 7000");
    Bus fresh;
    expect(fresh.in0 == 0xFF && fresh.in1 == 0xFF && fresh.dsw1 == 0xC9, "idle input values: IN0 = IN1 = FF, DSW1 default C9");

    // [I/O log] every write to the I/O area is logged with the frame number and the address as written
    Bus l;
    l.set_frame(7);
    l.write(0x5000, 1);
    l.write(0x50C0, 0);
    l.set_frame(8);
    l.write(0x5155, 3);
    l.write(0x5075, 3);
    l.write(0x4C00, 1);
    expect(l.io_log.size() == 4 && l.io_log[0].frame == 7 && l.io_log[0].address == 0x5000 && l.io_log[0].value == 1 && l.io_log[2].frame == 8 &&
               l.io_log[2].address == 0x5155 && l.io_log[3].address == 0x5075,
           "I/O writes logged in order (ignored ones too); RAM writes not logged");

    // [section 1 row interrupt vector] OUT (0),A sets the vector byte; IN gives FF
    Bus p;
    p.port_out(0x0000, 0xFA);
    expect(p.irq_vector == 0xFA, "OUT to port 0 sets the vector");
    p.port_out(0x0100, 0x12);
    expect(p.irq_vector == 0x12, "port decoded on the low 8 bits only");
    p.port_out(0x0001, 0x77);
    expect(p.irq_vector == 0x12, "other ports ignored");
    expect(p.port_in(0) == 0xFF, "IN returns FF");

    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS test_bus", checks, failures);
    return failures ? 1 : 0;
}
