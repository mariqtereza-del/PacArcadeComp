// N01: records which ROM instruction addresses the machine executes (the "progress meter" of Track D). FOR TESTS ONLY.
// Use with probe::run_frame (tests/logic_probe.h): the callback is called before every instruction, so mark(pc) counts every instruction address that runs.
// The file written by write() holds addresses only (hexadecimal ranges, one per line), never ROM bytes.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace romcov {

class Coverage {
public:
    Coverage() : seen_(0x4000, 0) {}
    void mark(uint16_t pc) {
        if (pc < 0x4000) seen_[pc] = 1; // an instruction outside the ROM (RAM) is not counted
    }
    bool seen(unsigned a) const { return a < 0x4000 && seen_[a]; }
    unsigned count() const {
        unsigned n = 0;
        for (uint8_t v : seen_) n += v;
        return n;
    }
    void merge(const Coverage &o) {
        for (size_t i = 0; i < seen_.size(); ++i) seen_[i] |= o.seen_[i];
    }
    // one line per run of consecutive addresses: "0000 0007" (first and last address, hexadecimal)
    bool write(const char *path) const {
        FILE *f = std::fopen(path, "wb");
        if (!f) return false;
        std::fprintf(f, "# N01 coverage: ranges of ROM instruction START addresses that were executed (addresses only)\n");
        for (unsigned a = 0; a < 0x4000;) {
            if (!seen_[a]) {
                ++a;
                continue;
            }
            unsigned b = a;
            while (b + 1 < 0x4000 && seen_[b + 1]) ++b;
            std::fprintf(f, "%04X %04X\n", a, b);
            a = b + 1;
        }
        std::fclose(f);
        return true;
    }

private:
    std::vector<uint8_t> seen_;
};

} // namespace romcov
