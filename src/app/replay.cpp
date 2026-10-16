// replay.cpp: input recording, replay and the per-frame state hash declared in replay.h.
#include "replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include "../board/bus.h"
#include "../board/machine.h"

bool Replay::load(const std::string &path, std::string &error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::string line;
    bool header = false;
    *this = Replay();
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        if (!header) {
            std::string a, b;
            ss >> a >> b;
            if (a != "PACMAN-REPLAY" || b != "1") {
                error = "not a PACMAN-REPLAY 1 file";
                return false;
            }
            header = true;
            continue;
        }
        std::string w;
        ss >> w;
        if (w == "dip") {
            unsigned v;
            ss >> std::hex >> v;
            dip = (uint8_t)v;
        } else if (w == "frames") {
            ss >> std::dec >> frames;
        } else {
            unsigned f = (unsigned)std::strtoul(w.c_str(), nullptr, 10), a, b;
            if (!(ss >> std::hex >> a >> b)) {
                error = "bad line: " + line;
                return false;
            }
            if (!entries.empty() && f <= entries.back().frame) {
                error = "frame numbers must increase: " + line;
                return false;
            }
            entries.push_back({f, (uint8_t)a, (uint8_t)b});
        }
    }
    if (!header) {
        error = "empty file";
        return false;
    }
    if (entries.empty() || entries[0].frame != 0) {
        error = "the first input line must be for frame 0";
        return false;
    }
    return true;
}

bool Replay::save(const std::string &path, std::string &error) const {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) {
        error = "cannot write " + path;
        return false;
    }
    std::fprintf(f, "PACMAN-REPLAY 1\ndip %02X\nframes %u\n# frame in0 in1 (hex, active low; a value holds until the next line)\n", dip, frames);
    for (const ReplayEntry &e : entries) std::fprintf(f, "%u %02X %02X\n", e.frame, e.in0, e.in1);
    std::fclose(f);
    return true;
}

void Replay::inputs_at(uint32_t frame, uint8_t &in0, uint8_t &in1) const {
    in0 = in1 = 0xFF;
    for (const ReplayEntry &e : entries) {
        if (e.frame > frame) break;
        in0 = e.in0;
        in1 = e.in1;
    }
}

bool Replay::operator==(const Replay &o) const {
    if (dip != o.dip || frames != o.frames || entries.size() != o.entries.size()) return false;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].frame != o.entries[i].frame || entries[i].in0 != o.entries[i].in0 || entries[i].in1 != o.entries[i].in1) return false;
    return true;
}

void Recorder::frame(uint8_t in0, uint8_t in1) {
    uint32_t n = r_.frames++;
    if (r_.entries.empty() || r_.entries.back().in0 != in0 || r_.entries.back().in1 != in1) r_.entries.push_back({n, in0, in1});
}

std::string state_hash(const Bus &bus) {
    uint8_t buf[0x1000 + 48];
    std::memcpy(buf, bus.videoram, 0x400);
    std::memcpy(buf + 0x400, bus.colorram, 0x400);
    std::memset(buf + 0x800, 0, 0x400);
    std::memcpy(buf + 0xC00, bus.workram, 0x3F0);
    std::memcpy(buf + 0xFF0, bus.spriteram, 0x10);
    std::memcpy(buf + 0x1000, bus.sound_regs, 0x20);
    std::memcpy(buf + 0x1020, bus.sprite_coords, 0x10);
    return sha1_hex(buf, sizeof buf);
}

// ---------------------------------------------------------------- command-line tool (compile with -DREPLAY_MAIN)
// usage: replay_tool PROG.BIN REPLAY LOG [--record OUT_REPLAY] [--rules readable]
//   Runs the replay on the C++ machine and writes "frame sha1" for every frame (frame = number of frames run so far).
//   With --record the inputs are passed through a Recorder and saved, to prove that recording then loading gives the same replay.
#ifdef REPLAY_MAIN
#include "../rules/rules.h" // T02a: --rules readable
int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: replay_tool PROG.BIN REPLAY LOG [--record OUT_REPLAY]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    Replay rp;
    std::string err;
    if (!rp.load(argv[2], err)) {
        std::fprintf(stderr, "replay: %s\n", err.c_str());
        return 2;
    }
    const char *rec_path = nullptr;
    bool readable = false; // T02a: --rules readable runs the readable rules (src/rules/rules.h) instead of the ROM routines
    for (int i = 4; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--record")) rec_path = argv[i + 1];
        if (!std::strcmp(argv[i], "--rules")) readable = !std::strcmp(argv[i + 1], "readable");
    }
    static Machine m(prog);
    if (readable) m.rules = rules::table();
    Recorder rec(rp.dip);
    FILE *log = std::fopen(argv[3], "wb");
    if (!log) {
        std::fprintf(stderr, "cannot write %s\n", argv[3]);
        return 2;
    }
    std::set<std::string> distinct;
    int last_state = -1, state_changes = 0, max_dots = 0, max_lives = 0, two_player_seen = 0, games = 0;
    uint32_t max_score = 0;
    for (uint32_t fr = 0; fr < rp.frames; ++fr) {
        uint8_t a, b;
        rp.inputs_at(fr, a, b);
        m.bus.in0 = a;
        m.bus.in1 = b;
        m.bus.dsw1 = rp.dip;
        rec.frame(a, b);
        m.run_frame();
        std::string h = state_hash(m.bus);
        std::fprintf(log, "%u %s\n", fr + 1, h.c_str());
        distinct.insert(h);
        int st = m.bus.read(0x4E00);
        if (st != last_state) {
            ++state_changes;
            if (st == 3) ++games;
            last_state = st;
        }
        if (m.bus.read(0x4E0E) > max_dots) max_dots = m.bus.read(0x4E0E);
        if (m.bus.read(0x4E14) > max_lives) max_lives = m.bus.read(0x4E14);
        if (m.bus.read(0x4E70)) two_player_seen = 1;
        uint32_t sc = 0;
        bool bcd = true;
        for (int i = 2; i >= 0; --i) {
            int v = m.bus.read(0x4E80 + i);
            if ((v >> 4) > 9 || (v & 15) > 9) bcd = false;
            sc = sc * 100 + (v >> 4) * 10 + (v & 15);
        }
        if (bcd && fr >= 400 && sc > max_score)
            max_score = sc; // 4E80-4E82: P1 score, BCD (frames before 400 are skipped: the boot-time RAM test leaves odd values there)
    }
    std::printf("replay played: %u frames, %zu distinct frame states, main state changed %d times, %d games started, most dots on a level %d, highest P1 score %u, "
                "two-player game seen: %s\n",
                rp.frames, distinct.size(), state_changes, games, max_dots, max_score, two_player_seen ? "yes" : "no");
    std::fclose(log);
    if (rec_path && !rec.replay().save(rec_path, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    return 0;
}
#endif
