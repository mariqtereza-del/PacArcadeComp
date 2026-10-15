// machine.cpp: the machine = Z80 core + bus + frame loop with the vertical-blank interrupt (see machine.h).
#include "machine.h"
#include <cstdio>
#include <cstring>

Machine::Machine(const std::vector<uint8_t> &program) { bus.load_program(program); }

void Machine::deliver_vblank_interrupt() {
    if (!(bus.irq_enabled() && on_get_iff1() && !on_is_int_disabled())) return;
    if (on_is_halted()) {
        on_set_is_halted(false);
        if (prototype_quirks) on_set_pc((on_get_pc() + 1) & 0xFFFF); // the Python harness's extra +1 (the core already points after HALT)
    }
    uint16_t sp = (uint16_t)(on_get_sp() - 2), pc = (uint16_t)on_get_pc();
    bus.write(sp, pc & 0xFF);
    bus.write((uint16_t)(sp + 1), pc >> 8);
    on_set_sp(sp);
    on_set_iff1(false);
    on_set_iff2(false);
    uint16_t t = (uint16_t)((on_get_i() << 8) | bus.irq_vector);
    on_set_pc((uint16_t)(bus.read(t) | (bus.read((uint16_t)(t + 1)) << 8)));
}

uint32_t Machine::run_frame() {
    bus.io_log
        .clear(); // S36 bug fix: the log of I/O writes is only for the Bus tests; nothing in the program reads it, and it grew by about 300 bytes every frame (60 MB per hour of play)
    ticks = 0;
    for (;;) {
        wrapped = false;
        on_step();
        if (ticks >= cycles_per_frame) break;
        if (prototype_quirks && wrapped) break;
    }
    ++frame;
    bus.set_frame(frame);
    deliver_vblank_interrupt();
    return frame;
}

// ---------------------------------------------------------------- SHA-1
namespace {
struct Sha1 {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buf[64];
    size_t blen = 0;
    uint64_t total = 0;
    static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
    void block(const uint8_t *p) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = t;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    void update(const uint8_t *p, size_t n) {
        total += n;
        while (n) {
            size_t k = 64 - blen;
            if (k > n) k = n;
            std::memcpy(buf + blen, p, k);
            blen += k;
            p += k;
            n -= k;
            if (blen == 64) {
                block(buf);
                blen = 0;
            }
        }
    }
    std::string finish() {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t z = 0;
        while (blen != 56) update(&z, 1);
        uint8_t l[8];
        for (int i = 0; i < 8; ++i) l[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(l, 8);
        char out[41];
        for (int i = 0; i < 5; ++i) std::snprintf(out + 8 * i, 9, "%08x", (unsigned)h[i]);
        return out;
    }
};
} // namespace
std::string sha1_hex(const uint8_t *data, size_t n) {
    Sha1 s;
    s.update(data, n);
    return s.finish();
}

// ---------------------------------------------------------------- hash dump / compare program (compile with -DMACHINE_DUMP_MAIN)
// usage: machine_dump PROG.BIN FRAMES [--quirks] [--compare tests/data/oracle_ram_hashes.txt]
// Prints "frame sha1" for RAM 4000-4FFF after each frame; with --compare checks against the [exact] (or, with --quirks, the
// [prototype_frames]) section of the oracle file and prints PASS or the first differing frame.
#ifdef MACHINE_DUMP_MAIN
#include <fstream>
#include <map>
#include <sstream>
int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: machine_dump PROG.BIN FRAMES [--quirks] [--compare FILE]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    int frames = std::atoi(argv[2]);
    bool quirks = false;
    const char *cmp = nullptr;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--quirks")) quirks = true;
        else if (!std::strcmp(argv[i], "--compare") && i + 1 < argc) cmp = argv[++i];
    }
    static Machine m(prog);
    m.prototype_quirks = quirks;
    std::map<int, std::string> want;
    if (cmp) {
        std::ifstream in(cmp);
        std::string line, sec;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == '[') {
                sec = line;
                continue;
            }
            if (sec != (quirks ? "[prototype_frames]" : "[exact]")) continue;
            std::istringstream ss(line);
            int n;
            std::string h;
            ss >> n >> h;
            want[n] = h;
        }
        if (want.empty()) {
            std::fprintf(stderr, "no hashes for this mode in %s\n", cmp);
            return 2;
        }
    }
    int bad = 0, first = 0;
    for (int fr = 1; fr <= frames; ++fr) {
        m.run_frame();
        uint8_t ram[0x1000];
        // Hash the stored cells (video, colour, the unused 4800-4BFF as zeros, work RAM, sprite RAM), as the oracle hashes its flat memory.
        std::memcpy(ram, m.bus.videoram, 0x400);
        std::memcpy(ram + 0x400, m.bus.colorram, 0x400);
        std::memset(ram + 0x800, 0, 0x400);
        std::memcpy(ram + 0xC00, m.bus.workram, 0x3F0);
        std::memcpy(ram + 0xFF0, m.bus.spriteram, 0x10);
        std::string h = sha1_hex(ram, sizeof ram);
        if (!cmp) std::printf("%d %s\n", fr, h.c_str());
        else if (want.count(fr) && want[fr] != h) {
            if (!bad++) first = fr;
            if (bad <= 3) std::printf("  frame %d: c++ %s, oracle %s\n", fr, h.c_str(), want[fr].c_str());
        } else if (!want.count(fr)) {
            std::printf("  frame %d missing in oracle file\n", fr);
            return 1;
        }
    }
    if (cmp) {
        if (bad) {
            std::printf("FAIL: %d of %d frames differ, first at frame %d\n", bad, frames, first);
            return 1;
        }
        std::printf("PASS: %d frame hashes identical (%s)\n", frames, quirks ? "prototype frame pattern (--quirks)" : "exact 50,688-cycle frames");
    }
    return 0;
}
#endif
