// N10: ROM data map. Runs the coverage players A, B, C and D (tests/coverage_player.h) and records every read of the program ROM that is NOT an instruction
// fetch (data reads), with the routine that did it, then groups the reads by the data blocks of docs/disasm/blocks.txt (step S08).
// How a data read is told from a fetch: this test is built with a copy of src/board/bus.cpp that reports every ROM read (the copy is made by the shell script
// with sed; the game source is not touched). In every Z80 step the first k reads are the instruction's own bytes (k comes from an independent instruction
// length table, checked below against the real program counter); any ROM read after them is a data read. Reads made outside a step (the interrupt vector
// lookup when the interrupt is delivered) are listed on their own. FOR TESTS ONLY. Output: addresses, sizes, counts; no ROM bytes.
// Usage: rom_data_map PROG.BIN ROUTINES.md BLOCKS.txt OUT.txt [framesA framesB framesC framesD]
#define run_frame run_frame_plain
#include "logic_probe.h"
#undef run_frame
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

void (*rom_read_hook)(uint16_t) = nullptr; // called by the test copy of Bus::read for every ROM read

namespace dm {

std::vector<uint16_t> step_reads;
bool in_step = false;
std::vector<uint16_t> outside_reads;
void hook(uint16_t off) {
    if (in_step) step_reads.push_back(off);
    else outside_reads.push_back(off);
}

struct Stats {
    uint64_t steps = 0, fetch_reads = 0, data_reads = 0, ctrl_steps = 0, repeat_steps = 0;
    uint64_t len_mismatch = 0, fetch_mismatch = 0, ctrl_with_data = 0, short_reads = 0;
    uint64_t outside = 0;
} S;
// kinds of read: 0 = the game and the self-test tables (normal), 1 = the ROM checksum loop of the power-on test, 2 = ROM bytes used as random numbers, 3 = the interrupt vector lookup
constexpr unsigned kRomSumPc = 0x300A, kRandomPc = 0x2A30; // the reading instructions of kinds 1 and 2 (addresses found by the first run of this tool, listed in section 4)
uint64_t cntk[4][0x4000];
uint64_t cnt[0x4000]; // all kinds together
std::vector<std::set<uint16_t>> readers(0x4000); // routine entries that read this address as data
std::map<uint16_t, uint64_t> reader_pc;           // instruction address -> data reads it made
std::map<uint16_t, std::set<uint16_t>> reader_pc_targets;
std::vector<std::pair<unsigned, unsigned>> g_routines_sorted;
uint16_t routine_entry[0x4000]; // filled from the routine map

bool pending_idx = false;

// length of the Z80 step that starts at pc (a DD or FD prefix is a step of its own); ctrl = the step is a jump, call, return, restart or relative jump
int step_len(const uint8_t *rom, unsigned pc, bool idx, bool &ctrl, bool &prefix_only, bool &repeat) {
    ctrl = prefix_only = repeat = false;
    const uint8_t op = rom[pc], op1 = rom[(pc + 1) & 0x3FFF];
    if (!idx && (op == 0xDD || op == 0xFD)) {
        prefix_only = true;
        return 1;
    }
    if (op == 0xCB) return idx ? 3 : 2;
    if (!idx && op == 0xED) {
        if (op1 == 0x4D || op1 == 0x45) ctrl = true;
        if (op1 >= 0xA0 && op1 <= 0xBB && (op1 & 4) == 0 && (op1 & 0x10)) repeat = true; // LDIR LDDR CPIR CPDR INIR INDR OTIR OTDR (one step may repeat)
        static const uint8_t four[8] = {0x43, 0x4B, 0x53, 0x5B, 0x63, 0x6B, 0x73, 0x7B};
        for (uint8_t f : four)
            if (op1 == f) return 4;
        return 2;
    }
    int len = 1;
    if (op == 0x01 || op == 0x11 || op == 0x21 || op == 0x31 || op == 0x22 || op == 0x2A || op == 0x32 || op == 0x3A || op == 0xC3 || op == 0xCD || (op & 0xC7) == 0xC2 ||
        (op & 0xC7) == 0xC4)
        len = 3;
    else if ((op & 0xC7) == 0x06 || op == 0x10 || op == 0x18 || op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38 || (op & 0xC7) == 0xC6 || op == 0xD3 || op == 0xDB)
        len = 2;
    if (op == 0xC3 || op == 0xCD || op == 0xC9 || op == 0xE9 || (op & 0xC7) == 0xC0 || (op & 0xC7) == 0xC2 || (op & 0xC7) == 0xC4 || (op & 0xC7) == 0xC7 || op == 0x10 || op == 0x18 ||
        op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38)
        ctrl = true;
    if (idx) { // after DD or FD: an operation on (HL) becomes (IX+d), one byte longer
        const bool uses_hl = op == 0x34 || op == 0x35 || op == 0x36 || ((op & 0xC7) == 0x46 && op != 0x76) || (op >= 0x70 && op <= 0x77 && op != 0x76) || ((op & 0xC7) == 0x86);
        if (uses_hl) len += 1;
    }
    return len;
}

} // namespace dm

namespace probe {
template <class Before> inline void run_frame(Machine &m, Before &&before) {
    using namespace dm;
    m.bus.io_log.clear();
    const uint32_t t0 = m.ticks_in_frame();
    for (;;) {
        const uint16_t pc = (uint16_t)m.on_get_pc();
        before(pc);
        bool ctrl, pre, rep;
        const int k = pc < 0x4000 ? step_len(m.bus.rom, pc, pending_idx, ctrl, pre, rep) : 1;
        const bool was_halted = m.on_is_halted(); // a halted CPU repeats a step that reads nothing
        step_reads.clear();
        in_step = true;
        m.on_step();
        in_step = false;
        if (pc < 0x4000 && !was_halted) {
            ++S.steps;
            const unsigned pc_after = m.on_get_pc();
            if (ctrl) ++S.ctrl_steps;
            if (rep) ++S.repeat_steps;
            const bool halt = m.bus.rom[pc] == 0x76;
            if (!ctrl && !rep && !halt && pc_after != ((pc + k) & 0xFFFF)) { ++S.len_mismatch; if (getenv("DM_DEBUG")) std::fprintf(stderr, "len pc=%04X op=%02X %02X k=%d after=%04X idx=%d\n", pc, m.bus.rom[pc], m.bus.rom[pc+1], k, pc_after, (int)pending_idx); }
            const size_t n = step_reads.size();
            const size_t nf = std::min<size_t>(n, (size_t)k);
            for (size_t i = 0; i < nf; ++i)
                if (step_reads[i] != ((pc + i) & 0x3FFF)) ++S.fetch_mismatch;
            if (n < (size_t)k) ++S.short_reads;
            S.fetch_reads += nf;
            for (size_t i = nf; i < n; ++i) {
                const uint16_t a = step_reads[i];
                if (ctrl) ++S.ctrl_with_data;
                ++S.data_reads;
                const int kind = pc == kRomSumPc ? 1 : pc == kRandomPc ? 2 : 0;
                ++cnt[a];
                ++cntk[kind][a];
                if (kind == 0) readers[a].insert(routine_entry[pc]);
                ++reader_pc[(uint16_t)pc];
                reader_pc_targets[(uint16_t)pc].insert(a);
            }
            pending_idx = pre ? true : false;
        }
        if (m.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
    }
    ++m.frame;
    m.bus.set_frame(m.frame);
    m.deliver_vblank_interrupt();
    S.outside += outside_reads.size();
    for (uint16_t a : outside_reads) {
        ++dm::cnt[a];
        ++dm::cntk[3][a];
    }
    outside_reads.clear();
}
} // namespace probe

#include "coverage_player.h"

namespace {
struct Block {
    std::string name, type;
    unsigned start, end;
};
} // namespace

int main(int argc, char **argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: rom_data_map PROG.BIN ROUTINES.md BLOCKS.txt OUT.txt [framesA framesB framesC framesD]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    // routine map: entry of the routine that holds each address
    std::vector<unsigned> entries;
    {
        std::ifstream in(argv[2]);
        std::string line;
        bool sec = false;
        while (std::getline(in, line)) {
            if (line.rfind("## ", 0) == 0) sec = line.rfind("## Routines", 0) == 0;
            if (!sec || line.size() < 8 || line[0] != '|' || line[1] != ' ') continue;
            char *end = nullptr;
            const unsigned long a = std::strtoul(line.c_str() + 2, &end, 16);
            if (end == line.c_str() + 6 && *end == ' ') entries.push_back((unsigned)a);
        }
        std::sort(entries.begin(), entries.end());
        if (entries.size() < 200) {
            std::fprintf(stderr, "bad routine map\n");
            return 2;
        }
        size_t j = 0;
        for (unsigned a = 0; a < 0x4000; ++a) {
            while (j + 1 < entries.size() && entries[j + 1] <= a) ++j;
            dm::routine_entry[a] = (uint16_t)entries[j];
        }
    }
    std::vector<Block> blocks;
    {
        std::ifstream in(argv[3]);
        std::string line;
        while (std::getline(in, line)) {
            char nm[64], ty[32];
            unsigned s, e;
            if (std::sscanf(line.c_str(), "%63[^:]: start 0x%x end 0x%x type %31s", nm, &s, &e, ty) == 4) blocks.push_back({nm, ty, s, e});
        }
        if (blocks.size() < 200) {
            std::fprintf(stderr, "bad blocks file\n");
            return 2;
        }
    }
    unsigned long fr[4] = {300000, 300000, 300000, 54000};
    for (int i = 0; i < 4 && 5 + i < argc; ++i) fr[i] = std::strtoul(argv[5 + i], nullptr, 10);
    rom_read_hook = dm::hook;
    {
        static Machine m(prog);
        covplayer::run(m, fr[0], nullptr);
    }
    covplayer::run_b(prog, fr[1], nullptr);
    covplayer::run_c(prog, fr[2], nullptr);
    covplayer::run_d(prog, fr[3], nullptr);
    rom_read_hook = nullptr;

    FILE *o = std::fopen(argv[4], "wb");
    if (!o) return 2;
    using dm::S;
    uint64_t total = 0;
    unsigned distinct = 0;
    for (unsigned a = 0; a < 0x4000; ++a) {
        total += dm::cnt[a];
        distinct += dm::cnt[a] != 0;
    }
    std::fprintf(o, "N10 ROM data map (tests/rom_data_map.cpp). Players A, B, C, D of tests/coverage_player.h: %lu %lu %lu %lu frames. Addresses, sizes and counts only.\n\n", fr[0], fr[1], fr[2], fr[3]);
    std::fprintf(o, "== 1. Totals\n");
    std::fprintf(o, "Z80 steps in the ROM %llu; instruction-fetch reads %llu; DATA reads %llu (from %zu reading instructions); reads made outside a step (interrupt vector lookup) %llu\n",
                 (unsigned long long)S.steps, (unsigned long long)S.fetch_reads, (unsigned long long)S.data_reads, dm::reader_pc.size(), (unsigned long long)S.outside);
    std::fprintf(o, "distinct ROM addresses read as data (or by the interrupt): %u\n", distinct);
    std::fprintf(o, "self-checks: steps whose length differs from the instruction length table %llu; steps whose first reads are not the instruction's own bytes %llu; "
                    "jump/call/return steps that also made a data read %llu; steps with fewer reads than their length %llu (all must be 0)\n",
                 (unsigned long long)S.len_mismatch, (unsigned long long)S.fetch_mismatch, (unsigned long long)S.ctrl_with_data, (unsigned long long)S.short_reads);

    std::fprintf(o, "\n== 2. Reads by data block of docs/disasm/blocks.txt (S08)\n");
    std::fprintf(o, "block | type | block range | size | distinct bytes read | data reads | ranges read (first-last) | reading routines (entry:reads)\n");
    uint64_t covered = 0, in_code = 0;
    unsigned tables = 0, codetables = 0;
    std::vector<bool> inblock(0x4000, false);
    for (const Block &b : blocks) {
        uint64_t reads = 0;
        unsigned dist = 0;
        std::string ranges;
        std::map<unsigned, uint64_t> rd;
        for (unsigned a = b.start; a < b.end && a < 0x4000; ++a) {
            inblock[a] = true;
            if (!dm::cntk[0][a]) continue;
            reads += dm::cntk[0][a];
            ++dist;
        }
        if (!reads) continue;
        const bool is_code = b.type == "code";
        for (unsigned a = b.start; a < b.end;) {
            if (!dm::cntk[0][a]) { ++a; continue; }
            unsigned z = a;
            while (z + 1 < b.end && dm::cntk[0][z + 1]) ++z;
            char r[24];
            std::snprintf(r, sizeof r, "%04X-%04X ", a, z);
            if (ranges.size() < 600) ranges += r;
            a = z + 1;
        }
        // readers: attribute each read to the routine of the instruction that made it
        for (unsigned a = b.start; a < b.end; ++a)
            for (uint16_t re : dm::readers[a]) rd[re] += dm::cntk[0][a] ? 1 : 0; // number of bytes of the block this routine read
        std::string rs;
        for (auto &p : rd) {
            char r[24];
            if (p.first == 0xFFFF) std::snprintf(r, sizeof r, "INT:%llu ", (unsigned long long)p.second);
            else std::snprintf(r, sizeof r, "%04X:%llu ", p.first, (unsigned long long)p.second);
            rs += r;
        }
        std::fprintf(o, "%s | %s | %04X-%04X | %u | %u | %llu | %s| %s\n", b.name.c_str(), b.type.c_str(), b.start, b.end - 1, b.end - b.start, dist, (unsigned long long)reads,
                     ranges.c_str(), rs.c_str());
        covered += reads;
        if (is_code) {
            in_code += reads;
            ++codetables;
        } else ++tables;
    }
    uint64_t outside_blocks = 0;
    for (unsigned a = 0; a < 0x4000; ++a) if (dm::cntk[0][a] && !inblock[a]) outside_blocks += dm::cntk[0][a];
    std::fprintf(o, "blocks with reads: %u data/table blocks, %u code blocks read as data; normal reads in data blocks %llu, normal reads of code bytes as data %llu, normal reads in no block %llu\n", tables,
                 codetables, (unsigned long long)(covered - in_code), (unsigned long long)in_code, (unsigned long long)outside_blocks);

    // 2b: the runs of bytes that are read by the same set of routines (this is the table list: one table = one run)
    std::fprintf(o, "\n== 2b. Tables: runs of consecutive bytes read by the same routines (normal reads)\n");
    std::fprintf(o, "first-last | size | block | reading routines | data reads | reading instructions\n");
    unsigned runs = 0;
    for (unsigned a = 0; a < 0x4000;) {
        if (!dm::cntk[0][a]) { ++a; continue; }
        unsigned z = a;
        while (z + 1 < 0x4000 && dm::cntk[0][z + 1] && dm::readers[z + 1] == dm::readers[a]) ++z;
        std::string blk;
        for (const Block &b : blocks) if (a >= b.start && a < b.end) blk = b.name;
        uint64_t n = 0;
        for (unsigned x = a; x <= z; ++x) n += dm::cntk[0][x];
        std::string rs;
        for (uint16_t re : dm::readers[a]) { char r[16]; std::snprintf(r, sizeof r, "%04X ", re); rs += r; }
        std::string ins;
        for (auto &p : dm::reader_pc_targets) {
            if (p.first == dm::kRomSumPc || p.first == dm::kRandomPc) continue;
            bool hit = false;
            for (unsigned x : p.second) if (x >= a && x <= z) { hit = true; break; }
            if (hit) { char r[16]; std::snprintf(r, sizeof r, "%04X ", p.first); ins += r; }
        }
        std::fprintf(o, "%04X-%04X | %u | %s | %s| %llu | %s\n", a, z, z - a + 1, blk.c_str(), rs.c_str(), (unsigned long long)n, ins.c_str());
        ++runs;
        a = z + 1;
    }
    std::fprintf(o, "runs: %u\n", runs);

    std::fprintf(o, "\n== 3. Reads of the other kinds (not part of the game tables)\n");
    const char *kn[4] = {"", "ROM checksum loop of the power-on test (reading instruction 300A)", "ROM bytes used as random numbers (reading instruction 2A30)", "interrupt vector lookup when the interrupt is delivered"};
    for (int k = 1; k < 4; ++k) {
        uint64_t n = 0;
        unsigned d = 0;
        std::string ranges;
        for (unsigned a = 0; a < 0x4000; ++a) {
            if (!dm::cntk[k][a]) continue;
            n += dm::cntk[k][a];
            ++d;
        }
        for (unsigned a = 0; a < 0x4000;) {
            if (!dm::cntk[k][a]) { ++a; continue; }
            unsigned z = a;
            while (z + 1 < 0x4000 && dm::cntk[k][z + 1]) ++z;
            char r[24];
            std::snprintf(r, sizeof r, "%04X-%04X ", a, z);
            if (ranges.size() < 400) ranges += r;
            a = z + 1;
        }
        std::fprintf(o, "%s: %llu reads, %u distinct addresses, ranges: %s\n", kn[k], (unsigned long long)n, d, ranges.c_str());
    }
    std::fprintf(o, "\n== 4. Reading instructions (address : data reads : lowest-highest address read)\n");
    for (auto &p : dm::reader_pc) {
        const auto &t = dm::reader_pc_targets[p.first];
        std::fprintf(o, "%04X : %llu : %04X-%04X : routine %04X\n", p.first, (unsigned long long)p.second, *t.begin(), *t.rbegin(), dm::routine_entry[p.first]);
    }
    std::fclose(o);
    std::printf("rom data map: %llu data reads, %u distinct addresses; written to %s\n", (unsigned long long)S.data_reads, distinct, argv[4]);
    return (S.len_mismatch || S.fetch_mismatch || S.ctrl_with_data || S.short_reads) ? 1 : 0;
}
