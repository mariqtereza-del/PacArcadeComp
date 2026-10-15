// S33: see state.h
#include "state.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace state {

static_assert(std::is_standard_layout<Wsg>::value, "the sound chip object must be standard layout (its waveform table is its first member)");
static const size_t kWsgPromBytes = 256;   // Wsg::prom_ is the first member, 256 bytes: left out of states (ROM data)
static const uint32_t kMagic = 0x54534D50; // "PMST"
static const uint16_t kVersion = 2;        // 2: a tag byte after the input ports; version 1 states (S33 first build) are still read, with tag 1

namespace {
struct Writer {
    std::vector<uint8_t> &o;
    void u8(unsigned v) { o.push_back((uint8_t)v); }
    void u16(unsigned v) {
        u8(v & 255);
        u8(v >> 8);
    }
    void u32(uint32_t v) {
        u16(v & 0xFFFF);
        u16(v >> 16);
    }
    void bytes(const uint8_t *p, size_t n) { o.insert(o.end(), p, p + n); }
};
struct Reader {
    const std::vector<uint8_t> &in;
    size_t p = 0;
    bool ok = true;
    unsigned u8() {
        if (p >= in.size()) {
            ok = false;
            return 0;
        }
        return in[p++];
    }
    unsigned u16() {
        unsigned a = u8();
        return a | (u8() << 8);
    }
    uint32_t u32() {
        uint32_t a = u16();
        return a | ((uint32_t)u16() << 16);
    }
    void bytes(uint8_t *d, size_t n) {
        if (p + n > in.size()) {
            ok = false;
            std::memset(d, 0, n);
            return;
        }
        std::memcpy(d, in.data() + p, n);
        p += n;
    }
};
uint32_t crc32(const uint8_t *d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= d[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
} // namespace

void capture(Machine &m, const Wsg &wsg, std::vector<uint8_t> &out, uint8_t tag) {
    out.clear();
    out.reserve(2600);
    Writer w{out};
    w.u32(kMagic);
    w.u16(kVersion);
    // processor
    w.u16(m.on_get_af());
    w.u16(m.on_get_bc());
    w.u16(m.on_get_de());
    w.u16(m.on_get_hl());
    w.u16(m.get_alt_af());
    w.u16(m.get_alt_bc());
    w.u16(m.get_alt_de());
    w.u16(m.get_alt_hl());
    w.u16(m.on_get_ix());
    w.u16(m.on_get_iy());
    w.u16(m.on_get_sp());
    w.u16(m.on_get_pc());
    w.u16(m.on_get_wz());
    w.u8(m.on_get_i());
    w.u8(m.on_get_r());
    w.u8(m.on_get_iff1());
    w.u8(m.on_get_iff2());
    w.u8(m.on_get_int_mode());
    w.u8(m.on_is_halted());
    w.u8(m.on_is_int_disabled());
    w.u32(m.frame);
    // board
    const Bus &b = m.bus;
    w.bytes(b.videoram, sizeof b.videoram);
    w.bytes(b.colorram, sizeof b.colorram);
    w.bytes(b.workram, sizeof b.workram);
    w.bytes(b.spriteram, sizeof b.spriteram);
    w.bytes(b.sprite_coords, sizeof b.sprite_coords);
    w.bytes(b.sound_regs, sizeof b.sound_regs);
    w.bytes(b.latch, sizeof b.latch);
    w.u8(b.irq_vector);
    w.u8(b.in0);
    w.u8(b.in1);
    w.u8(b.dsw1);
    w.u8(b.dsw2);
    w.u8(tag);
    // sound chip: its object without the waveform table, then what its public getters say (checked when loading)
    const uint8_t *wp = reinterpret_cast<const uint8_t *>(&wsg);
    w.u16((unsigned)(sizeof(Wsg) - kWsgPromBytes));
    w.bytes(wp + kWsgPromBytes, sizeof(Wsg) - kWsgPromBytes);
    for (int v = 0; v < 3; ++v) {
        w.u32(wsg.accumulator(v));
        w.u32(wsg.frequency(v));
        w.u8(wsg.volume(v));
        w.u8(wsg.waveform(v));
    }
    w.u32(crc32(out.data(), out.size()));
}

bool restore(Machine &m, Wsg &wsg, const std::vector<uint8_t> &in, std::string *error, uint8_t *tag_out) {
    auto fail = [&](const char *why) {
        if (error) *error = why;
        return false;
    };
    if (in.size() < 12) return fail("not a saved state (too short)");
    uint32_t want = (uint32_t)in[in.size() - 4] | ((uint32_t)in[in.size() - 3] << 8) | ((uint32_t)in[in.size() - 2] << 16) | ((uint32_t)in[in.size() - 1] << 24);
    if (crc32(in.data(), in.size() - 4) != want) return fail("the saved state is damaged (checksum differs)");
    Reader r{in};
    if (r.u32() != kMagic) return fail("not a saved state of this program");
    unsigned ver = r.u16();
    if (ver != 1 && ver != kVersion) return fail("the saved state is from a different version of the program");
    // read everything into temporaries first, so a bad file changes nothing
    unsigned af = r.u16(), bc = r.u16(), de = r.u16(), hl = r.u16(), aaf = r.u16(), abc = r.u16(), ade = r.u16(), ahl = r.u16();
    unsigned ix = r.u16(), iy = r.u16(), sp = r.u16(), pc = r.u16(), wz = r.u16();
    unsigned ri = r.u8(), rr = r.u8();
    unsigned iff1 = r.u8(), iff2 = r.u8(), im = r.u8(), halted = r.u8(), intdis = r.u8();
    uint32_t frame = r.u32();
    static uint8_t vram[0x400], cram[0x400], wram[0x3F0], sram[0x10], scoord[0x10], sreg[0x20], latch[8];
    r.bytes(vram, sizeof vram);
    r.bytes(cram, sizeof cram);
    r.bytes(wram, sizeof wram);
    r.bytes(sram, sizeof sram);
    r.bytes(scoord, sizeof scoord);
    r.bytes(sreg, sizeof sreg);
    r.bytes(latch, sizeof latch);
    unsigned irqv = r.u8(), in0 = r.u8(), in1 = r.u8(), dsw1 = r.u8(), dsw2 = r.u8();
    unsigned tag = ver >= 2 ? r.u8() : 1;
    unsigned wlen = r.u16();
    if (wlen != sizeof(Wsg) - kWsgPromBytes) return fail("the saved state was made by a different build of the program");
    std::vector<uint8_t> wbytes(wlen);
    r.bytes(wbytes.data(), wlen);
    uint32_t acc[3], fq[3];
    unsigned vol[3], wave[3];
    for (int v = 0; v < 3; ++v) {
        acc[v] = r.u32();
        fq[v] = r.u32();
        vol[v] = r.u8();
        wave[v] = r.u8();
    }
    if (!r.ok) return fail("the saved state is cut short");
    if (tag_out) *tag_out = (uint8_t)tag;
    // apply
    m.on_set_af(af);
    m.on_set_bc(bc);
    m.on_set_de(de);
    m.on_set_hl(hl);
    m.set_alt_af(aaf);
    m.set_alt_bc(abc);
    m.set_alt_de(ade);
    m.set_alt_hl(ahl);
    m.on_set_ix(ix);
    m.on_set_iy(iy);
    m.on_set_sp(sp);
    m.on_set_pc(pc);
    m.on_set_wz(wz);
    m.on_set_i(ri);
    m.on_set_r(rr);
    m.on_set_iff1(iff1 != 0);
    m.on_set_iff2(iff2 != 0);
    m.on_set_int_mode(im);
    m.on_set_is_halted(halted != 0);
    m.on_set_is_int_disabled(intdis != 0);
    m.frame = frame;
    m.bus.set_frame(frame);
    Bus &b = m.bus;
    std::memcpy(b.videoram, vram, sizeof vram);
    std::memcpy(b.colorram, cram, sizeof cram);
    std::memcpy(b.workram, wram, sizeof wram);
    std::memcpy(b.spriteram, sram, sizeof sram);
    std::memcpy(b.sprite_coords, scoord, sizeof scoord);
    std::memcpy(b.sound_regs, sreg, sizeof sreg);
    std::memcpy(b.latch, latch, sizeof latch);
    b.irq_vector = (uint8_t)irqv;
    b.in0 = (uint8_t)in0;
    b.in1 = (uint8_t)in1;
    b.dsw1 = (uint8_t)dsw1;
    b.dsw2 = (uint8_t)dsw2;
    std::memcpy(reinterpret_cast<uint8_t *>(&wsg) + kWsgPromBytes, wbytes.data(), wlen);
    for (int v = 0; v < 3; ++v)
        if (wsg.accumulator(v) != acc[v] || wsg.frequency(v) != fq[v] || (unsigned)wsg.volume(v) != vol[v] || (unsigned)wsg.waveform(v) != wave[v]) {
            if (error) *error = "internal check failed: the sound chip state did not come back as saved";
            return false;
        }
    return true;
}

bool save_file(const std::string &path, const std::vector<uint8_t> &bytes, std::string *error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (path.empty()) {
        if (error) *error = "no settings folder found";
        return false;
    }
    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (error) *error = "cannot write " + tmp;
            return false;
        }
        f.write((const char *)bytes.data(), (std::streamsize)bytes.size());
        if (!f) {
            if (error) *error = "write failed " + tmp;
            return false;
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
    }
    if (ec) {
        if (error) *error = "cannot replace " + path + ": " + ec.message();
        return false;
    }
    return true;
}
bool load_file(const std::string &path, std::vector<uint8_t> &bytes, std::string *error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "there is no saved state yet (" + path + ")";
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

void RewindBuffer::push(const std::vector<uint8_t> &s) {
    if (cap_ == 0) return;
    if (ring_.size() < cap_) ring_.resize(cap_);
    ring_[head_] = s;
    head_ = (head_ + 1) % cap_;
    if (count_ < cap_) ++count_;
}
bool RewindBuffer::pop(std::vector<uint8_t> &s) {
    if (count_ == 0) return false;
    head_ = (head_ + cap_ - 1) % cap_;
    s = ring_[head_];
    --count_;
    return true;
}

// ---- high score
static unsigned bcd(unsigned b) { return (b >> 4) * 10 + (b & 15); }
static unsigned to_bcd(unsigned v) { return ((v / 10) << 4) | (v % 10); }
static uint32_t three_bytes(const Bus &bus, uint16_t a) {
    // bus.read is non-const (it may have side effects on I/O); RAM is read straight from the array
    return bcd(bus.workram[a + 2 - 0x4C00]) * 10000u + bcd(bus.workram[a + 1 - 0x4C00]) * 100u + bcd(bus.workram[a - 0x4C00]);
}
uint32_t high_score_ram(const Bus &bus) { return three_bytes(bus, 0x4E88); }
uint32_t score_ram(const Bus &bus, int player) { return three_bytes(bus, player ? 0x4E84 : 0x4E80); }
void high_score_put(Bus &bus, uint32_t score) {
    if (score > 999999) score = 999999;
    bus.workram[0x4E88 - 0x4C00] = (uint8_t)to_bcd(score % 100);
    bus.workram[0x4E89 - 0x4C00] = (uint8_t)to_bcd(score / 100 % 100);
    bus.workram[0x4E8A - 0x4C00] = (uint8_t)to_bcd(score / 10000);
    // the ROM's digit routine (2ABE / 2ACE), called with HL = 43F2, c = 4: six digits from the top byte down, leading zeros blank except the last two digits
    int hl = 0x43F2, c = 4;
    for (int byte = 2; byte >= 0; --byte) {
        unsigned v = bus.workram[0x4E88 + byte - 0x4C00];
        for (int half = 0; half < 2; ++half) {
            unsigned n = half == 0 ? (v >> 4) : (v & 15);
            unsigned tile;
            if (n) {
                c = 0;
                tile = n;
            } else if (c == 0) tile = 0;
            else {
                tile = 0x40;
                --c;
            }
            bus.videoram[hl - 0x4000] = (uint8_t)tile;
            --hl;
        }
    }
}
bool high_score_read_file(const std::string &path, uint32_t *score) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        size_t h = line.find('#');
        if (h != std::string::npos) line.erase(h);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!k.empty() && std::isspace((unsigned char)k.back())) { k.pop_back(); }
        while (!k.empty() && std::isspace((unsigned char)k.front())) { k.erase(k.begin()); }
        if (k != "high_score") continue;
        char *end = nullptr;
        long n = std::strtol(v.c_str(), &end, 10);
        if (end == v.c_str() || n < 0 || n > 999999) return false;
        *score = (uint32_t)n;
        return true;
    }
    return false;
}
bool high_score_write_file(const std::string &path, uint32_t score, std::string *error) {
    std::ostringstream o;
    o << "# Pac-Man port: the saved high score (written when a game ends, only the score reached before any load or rewind in that game counts).\nhigh_score = " << score
      << "\n";
    std::string t = o.str();
    std::vector<uint8_t> b(t.begin(), t.end());
    return save_file(path, b, error);
}

HighScoreKeeper::HighScoreKeeper(const std::string &path) : path_(path) {
    uint32_t s = 0;
    if (!path.empty() && high_score_read_file(path, &s)) stored_ = s;
}

void HighScoreKeeper::commit(Machine &m) {
    // The number that may become the record: the best score reached in plain play (legit_best_: it starts again at every new game and is forgotten when a state is loaded)
    // or the high score of the last state loaded, as it was when that state was saved (floor_). Whatever is scored after a load or rewind never counts.
    uint32_t final_score = std::max(score_ram(m.bus, 0), score_ram(m.bus, 1)), cand = std::max(legit_best_, floor_);
    if (cand > stored_) {
        std::string err;
        stored_ = cand;
        msg_ = high_score_write_file(path_, cand, &err) ? "High score: new record " + std::to_string(cand) + " saved to " + path_
                                                        : "High score: could not save (" + err + ")";
        if (assisted() && final_score > cand) msg_ += " (the " + std::to_string(final_score) + " reached after a load or rewind does not count)";
    } else if (assisted() && final_score > stored_) msg_ = "High score: this game used save/load or rewind, so its " + std::to_string(final_score) + " is not saved";
}
void HighScoreKeeper::after_tick(Machine &m) {
    int s = m.bus.workram[0x4E00 - 0x4C00];
    if (!loaded_) {
        // Power-on: the ROM tests its RAM (frames 0-250: the state byte counts through garbage values) and then clears it (state 0), after which the attract screen
        // starts (state 1). Only then does the high score go in, once; before that nothing here looks at the memory.
        if (s == 0 && m.frame >= 100) armed_ = true;
        if (armed_ && s == 1) {
            loaded_ = true;
            if (stored_ > 0) {
                high_score_put(m.bus, stored_);
                msg_ = "High score: loaded " + std::to_string(stored_);
            }
        }
        prev_state_ = s;
        return;
    }
    if (prev_state_ != 3 && s == 3) {
        rule_.new_game();
        legit_best_ = 0;
    } // a new game began: it counts again
    if (s == 3 && !assisted()) legit_best_ = std::max(legit_best_, std::max(score_ram(m.bus, 0), score_ram(m.bus, 1)));
    if (prev_state_ == 3 && s != 3) commit(m);
    prev_state_ = s;
}
void HighScoreKeeper::note_state_replaced(Machine &m) { prev_state_ = m.bus.workram[0x4E00 - 0x4C00]; }
void HighScoreKeeper::note_state_loaded(Machine &m, uint8_t tag, uint32_t hs_in_state) {
    rule_.mark(assist::kLoad);
    legit_best_ = 0; // what was reached before the load is not kept: the state's own high score takes its place
    if (tag == 0)
        floor_ = hs_in_state; // a state saved in plain play: its high score counts. (A state saved after an earlier load/rewind or an old-format state: it does not.)
    note_state_replaced(m);
}
void HighScoreKeeper::finish(Machine &m) { commit(m); }

// ---- X12: save slots
static const uint32_t kSlotMagic = 0x4C534D50; // "PMSL"
static const size_t kSlotHeader = 4 + 1 + 1 + 1 + 1 + 4 + 8;
void slot_pack(Machine &m, const std::vector<uint8_t> &st, int64_t time, std::vector<uint8_t> &out) {
    int player = m.bus.read(0x4E09) ? 2 : 1;
    uint32_t score = score_ram(m.bus, player - 1);
    int level = (int)m.bus.read(0x4E13) + 1, lives = (int)m.bus.read(0x4E14);
    out.clear();
    Writer w{out};
    w.u32(kSlotMagic);
    w.u8(1);
    w.u8(player);
    w.u8(lives & 255);
    w.u8(std::min(level, 255));
    w.u32(score);
    for (int i = 0; i < 8; ++i) w.u8((unsigned)((uint64_t)time >> (8 * i)) & 255);
    out.insert(out.end(), st.begin(), st.end());
}
bool slot_unpack(const std::vector<uint8_t> &in, std::vector<uint8_t> &st, SlotInfo *info) {
    SlotInfo si;
    if (in.empty()) {
        if (info) *info = si;
        return false;
    }
    si.present = true;
    uint32_t magic = in.size() >= 4 ? (uint32_t)in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24) : 0;
    if (magic == kSlotMagic && in.size() >= kSlotHeader && in[4] == 1) {
        si.player = in[5];
        si.lives = in[6];
        si.level = in[7];
        si.score = (uint32_t)in[8] | (in[9] << 8) | (in[10] << 16) | ((uint32_t)in[11] << 24);
        uint64_t t = 0;
        for (int i = 0; i < 8; ++i) t |= (uint64_t)in[12 + i] << (8 * i);
        si.time = (int64_t)t;
        st.assign(in.begin() + kSlotHeader, in.end());
    } else {
        si.legacy = true;
        st = in;
    }
    if (info) *info = si;
    return true;
}
std::string slot_path(const std::string &dir, int n) {
    return (std::filesystem::path(dir) / (n == 1 ? std::string("quicksave.sav") : "slot" + std::to_string(n) + ".sav")).string();
}
bool slot_info(const std::string &path, SlotInfo *info) {
    SlotInfo si;
    if (info) *info = si;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> head(kSlotHeader + 4);
    f.read((char *)head.data(), (std::streamsize)head.size());
    head.resize((size_t)f.gcount());
    if (head.empty()) return false;
    std::vector<uint8_t> dummy;
    slot_unpack(head, dummy, &si);
    if (si.legacy) {
        std::error_code ec;
        auto ft = std::filesystem::last_write_time(path, ec);
        if (!ec) {
            auto sctp =
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            si.time = (int64_t)std::chrono::system_clock::to_time_t(sctp);
        }
    }
    if (info) *info = si;
    return true;
}
std::string slot_line(int n, const SlotInfo &si) {
    std::string s = std::to_string(n) + " ";
    if (!si.present) return s + "EMPTY";
    char d[32] = "";
    {
        time_t t = (time_t)si.time;
        struct tm tmv;
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        std::strftime(d, sizeof d, "%m-%d %H%M", &tmv);
    }
    if (si.legacy) return s + "OLD SAVE " + d;
    s += d;
    s += " L" + std::to_string(std::min(si.level, 99)) + " " + std::to_string(si.score);
    return s.substr(0, 24);
}

} // namespace state
