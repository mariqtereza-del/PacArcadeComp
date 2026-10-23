// N11: the ROM data reader (src/data/rom_data.*) returns exactly the ROM bytes of the tables listed by step N10.
// Usage: test_rom_reader PROG.BIN docs/notes/rom_data_tables.md build/n10/data_map.txt
//  1. every one of the 23 tables: same address, same size as the N10 list, and every byte equals the byte of the ROM file (the comparison is only here, in the test)
//  2. every run of data reads found by the N10 coverage runs (report section 2b, jump-table and inline blocks excluded) lies inside a table of the reader
//  3. the special reads (checksum range, random-number source, interrupt vector) and a deliberately wrong ROM image (shifted by one byte) is caught
#include "../src/data/rom_data.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace romdata;

static std::string slurp(const char *p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }

static unsigned compare_all(const RomData &rd, const std::vector<uint8_t> &prog, unsigned &bytes) {
    unsigned bad = 0;
    for (unsigned t = 0; t < static_cast<unsigned>(Table::Count); ++t) {
        View v = rd.table(static_cast<Table>(t));
        for (unsigned i = 0; i < v.size; ++i, ++bytes)
            if (v[i] != prog[v.first + i] || v.at_address(v.first + i) != prog[v.first + i]) ++bad;
    }
    return bad;
}

int main(int argc, char **argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: test_rom_reader PROG.BIN rom_data_tables.md data_map.txt\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
    std::fclose(f);
    RomData rd(prog.data());
    int fails = 0;

    // 1. positions equal the N10 list; bytes equal the ROM
    std::string md = slurp(argv[2]);
    std::regex row(R"(\n\| (T\d\d) \| ([0-9A-F]{4})-([0-9A-F]{4}) \|)");
    unsigned n = 0;
    for (std::sregex_iterator it(md.begin(), md.end(), row), e; it != e; ++it, ++n) {
        if (n >= static_cast<unsigned>(Table::Count)) { ++n; break; }
        const TableInfo &ti = info(static_cast<Table>(n));
        unsigned lo = std::strtoul((*it)[2].str().c_str(), nullptr, 16), hi = std::strtoul((*it)[3].str().c_str(), nullptr, 16);
        if ((*it)[1].str() != ti.id || ti.first != lo || static_cast<unsigned>(ti.first + ti.size - 1) != hi) {
            std::printf("FAIL: table %s of the reader (%04X, %u bytes) differs from the N10 list (%s %04X-%04X)\n", ti.id, ti.first, ti.size, (*it)[1].str().c_str(), lo, hi);
            ++fails;
        }
    }
    if (n != static_cast<unsigned>(Table::Count)) { std::printf("FAIL: the N10 list has %u tables, the reader %u\n", n, static_cast<unsigned>(Table::Count)); ++fails; }
    unsigned bytes = 0, bad = compare_all(rd, prog, bytes);
    std::printf("part 1: %u tables, %u bytes compared with the ROM file, %u different\n", static_cast<unsigned>(Table::Count), bytes, bad);
    if (bad) ++fails;

    // 2. every data-read run of the coverage runs is inside a table of the reader
    std::string rep = slurp(argv[3]);
    size_t a = rep.find("== 2b."), z = rep.find("== 3.");
    if (a == std::string::npos || z == std::string::npos) { std::printf("FAIL: no section 2b in %s\n", argv[3]); return 1; }
    std::string sec = rep.substr(a, z - a);
    std::regex run(R"(\n([0-9A-F]{4})-([0-9A-F]{4}) \| \d+ \| (\S*) \|)");
    unsigned runs = 0, skipped = 0, outside = 0, wrongbyte = 0;
    for (std::sregex_iterator it(sec.begin(), sec.end(), run), e; it != e; ++it) {
        std::string blk = (*it)[3].str();
        if (blk.rfind("jumptable", 0) == 0 || blk.rfind("inline", 0) == 0) { ++skipped; continue; }
        unsigned lo = std::strtoul((*it)[1].str().c_str(), nullptr, 16), hi = std::strtoul((*it)[2].str().c_str(), nullptr, 16);
        ++runs;
        for (unsigned ad = lo; ad <= hi; ++ad) {
            bool found = false;
            for (unsigned t = 0; t < static_cast<unsigned>(Table::Count) && !found; ++t) {
                View v = rd.table(static_cast<Table>(t));
                if (v.contains(ad)) { found = true; if (v.at_address(ad) != prog[ad]) ++wrongbyte; }
            }
            if (!found) { ++outside; std::printf("  address %04X (block %s) is in no table of the reader\n", ad, blk.c_str()); break; }
        }
    }
    std::printf("part 2: %u data-read runs of the coverage runs checked (%u jump-table / inline runs belong to the routines, not to this reader); not inside a reader table: %u; wrong byte: %u\n", runs, skipped, outside, wrongbyte);
    if (runs < 100 || outside || wrongbyte) ++fails;

    // 3. special reads and a deliberately wrong ROM image
    bool s_ok = rd.checksum_range_end() == 0x3000 && rd.interrupt_vector().size == 4 && rd.interrupt_vector().first == 0x3FFA;
    for (unsigned ad = 0; ad < 0x4000 && s_ok; ++ad) s_ok = rd.random_source_byte(static_cast<uint16_t>(ad)) == prog[ad];
    std::printf("part 3: special reads (checksum end 3000, vector 3FFA-3FFD, random source over all 16384 addresses): %s\n", s_ok ? "ok" : "WRONG");
    if (!s_ok) ++fails;
    RomData shifted(prog.data() + 1);   // a reader on a ROM that is one byte off must be caught by the comparison
    unsigned b2 = 0, bad2 = compare_all(shifted, prog, b2);
    std::printf("part 3b: a wrong ROM image (shifted by one byte): caught, %u of %u bytes differ\n", bad2, b2);
    if (bad2 < b2 / 2) { std::printf("FAIL: the wrong image was not caught\n"); ++fails; }

    std::printf(fails ? "FAIL test_rom_reader (%d)\n" : "PASS test_rom_reader (%d)\n", fails);
    return fails ? 1 : 0;
}
