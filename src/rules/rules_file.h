// T02i: the rules file (rules.cfg). It changes the numbers in the readable rules without any programming: speeds, scatter/chase times, frightened times,
// Cruise Elroy limits, ghost house limits, the points and the extra-life score.
//
// How it works: nothing in src/logic is changed. The readable rules run exactly as proved; where a number of the file differs from the standard one, the game
// puts the file's number in right after the readable rules have set the level up (levels) or in place of the points table (scores). With the standard file
// (or no file) nothing differs and the game is the same as before, frame for frame.
//
// File format: one comment line (starting with #) before each value, then one `key value` line. A key may end in `.N` (N = level 1 to 21) to change only
// that level; level 21 stands for 21 and every later level. A line that is wrong is reported and ignored; the other lines are still used.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../logic/scoring.h"
#include "../logic/mode_timers.h"

namespace rules {

constexpr int kLevels = 22; // index 1..21 (0 unused)

struct Tuning {
    bool active = false; // a rules file with at least one value that differs from the standard is in use
    // ---- per level (percent, or a count; -1 = the original value of that level)
    int pacman_speed[kLevels], ghost_speed[kLevels], phase_time[kLevels], frightened_time[kLevels];
    int elroy1[kLevels], elroy2[kLevels], pinky_dots[kLevels], inky_dots[kLevels], clyde_dots[kLevels];
    // ---- points (index = logic::PointsIndex)
    int points[logic::kPointsKinds];
    int extra_life_score = 0; // 0 = the DIP switch setting (10000, 15000 or 20000), else this exact score
    Tuning() { reset(); }
    void reset() {
        active = false;
        for (int l = 0; l < kLevels; ++l) {
            pacman_speed[l] = ghost_speed[l] = phase_time[l] = frightened_time[l] = 100;
            elroy1[l] = elroy2[l] = pinky_dots[l] = inky_dots[l] = clyde_dots[l] = -1;
        }
        for (unsigned i = 0; i < logic::kPointsKinds; ++i) points[i] = (int)logic::kPointsValue[i];
        extra_life_score = 0;
    }
    bool is_standard() const {
        Tuning s;
        return std::memcmp(this->pacman_speed, s.pacman_speed, sizeof pacman_speed) == 0 && std::memcmp(ghost_speed, s.ghost_speed, sizeof ghost_speed) == 0 &&
               std::memcmp(phase_time, s.phase_time, sizeof phase_time) == 0 && std::memcmp(frightened_time, s.frightened_time, sizeof frightened_time) == 0 &&
               std::memcmp(elroy1, s.elroy1, sizeof elroy1) == 0 && std::memcmp(elroy2, s.elroy2, sizeof elroy2) == 0 &&
               std::memcmp(pinky_dots, s.pinky_dots, sizeof pinky_dots) == 0 && std::memcmp(inky_dots, s.inky_dots, sizeof inky_dots) == 0 &&
               std::memcmp(clyde_dots, s.clyde_dots, sizeof clyde_dots) == 0 && std::memcmp(points, s.points, sizeof points) == 0 && extra_life_score == s.extra_life_score;
    }
};

// The one tuning the hooks look at (the game fills it once at start-up; Purist mode never runs the hooks at all).
inline Tuning &tuning() {
    static Tuning t;
    return t;
}

// ---- the keys
enum class Kind { LevelPercent, LevelCount, Points, ExtraLife };
struct KeyDef {
    const char *key;
    Kind kind;
    int slot;            // which array / which points kind
    int lo, hi;          // allowed range
    const char *comment; // the comment line written before the value
};
inline const std::vector<KeyDef> &key_table() {
    using logic::PointsIndex;
    static const std::vector<KeyDef> t = {
        {"pacman_speed_percent", Kind::LevelPercent, 0, 20, 200, "Pac-Man's speed in percent of the original (100 = original; 80 = slower, 120 = faster). Add .N for one level, e.g. pacman_speed_percent.3"},
        {"ghost_speed_percent", Kind::LevelPercent, 1, 20, 200, "The ghosts' speed (normal, frightened and tunnel) in percent of the original (100 = original)"},
        {"phase_time_percent", Kind::LevelPercent, 2, 10, 1000, "How long the scatter and chase phases last, in percent of the original (50 = half as long)"},
        {"frightened_time_percent", Kind::LevelPercent, 3, 0, 1000, "How long the ghosts stay blue after an energizer, in percent of the original (0 = they are not frightened at all)"},
        {"elroy1_dots_left", Kind::LevelCount, 0, 0, 244, "Blinky's first speed-up (Cruise Elroy) starts when this many dots are left (-1 = the original number of the level)"},
        {"elroy2_dots_left", Kind::LevelCount, 1, 0, 244, "Blinky's second speed-up starts when this many dots are left (-1 = original)"},
        {"pinky_house_dots", Kind::LevelCount, 2, 0, 244, "Dots Pac-Man must eat before Pinky leaves the ghost house (-1 = original; Pinky is normally out at once)"},
        {"inky_house_dots", Kind::LevelCount, 3, 0, 244, "Dots Pac-Man must eat before Inky leaves the ghost house (-1 = original)"},
        {"clyde_house_dots", Kind::LevelCount, 4, 0, 244, "Dots Pac-Man must eat before Clyde leaves the ghost house (-1 = original)"},
        {"dot_points", Kind::Points, PointsIndex::kDot, 0, 99990, "Points for a dot (a multiple of 10)"},
        {"energizer_points", Kind::Points, PointsIndex::kEnergizer, 0, 99990, "Points for an energizer (a multiple of 10)"},
        {"ghost1_points", Kind::Points, PointsIndex::kGhost1, 0, 99990, "Points for the first ghost eaten with one energizer"},
        {"ghost2_points", Kind::Points, PointsIndex::kGhost2, 0, 99990, "Points for the second ghost"},
        {"ghost3_points", Kind::Points, PointsIndex::kGhost3, 0, 99990, "Points for the third ghost"},
        {"ghost4_points", Kind::Points, PointsIndex::kGhost4, 0, 99990, "Points for the fourth ghost"},
        {"cherry_points", Kind::Points, PointsIndex::kCherry, 0, 99990, "Points for the cherry"},
        {"strawberry_points", Kind::Points, PointsIndex::kStrawberry, 0, 99990, "Points for the strawberry"},
        {"orange_points", Kind::Points, PointsIndex::kOrange, 0, 99990, "Points for the orange"},
        {"apple_points", Kind::Points, PointsIndex::kApple, 0, 99990, "Points for the apple"},
        {"melon_points", Kind::Points, PointsIndex::kMelon, 0, 99990, "Points for the melon"},
        {"galaxian_points", Kind::Points, PointsIndex::kGalaxian, 0, 99990, "Points for the Galaxian flagship"},
        {"bell_points", Kind::Points, PointsIndex::kBell, 0, 99990, "Points for the bell"},
        {"key_points", Kind::Points, PointsIndex::kKey, 0, 99990, "Points for the key"},
        {"extra_life_score", Kind::ExtraLife, 0, 0, 990000, "The score that gives the extra life (0 = the BONUS LIFE setting of the menu: 10000, 15000 or 20000; a multiple of 10)"},
    };
    return t;
}

inline int* level_array(Tuning &t, Kind k, int slot) {
    if (k == Kind::LevelPercent) return slot == 0 ? t.pacman_speed : slot == 1 ? t.ghost_speed : slot == 2 ? t.phase_time : t.frightened_time;
    return slot == 0 ? t.elroy1 : slot == 1 ? t.elroy2 : slot == 2 ? t.pinky_dots : slot == 3 ? t.inky_dots : t.clyde_dots;
}

inline std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

// Reads the text of a rules file. Every wrong line is added to `problems` ("line 7: ...") and ignored. Returns the number of values that were taken.
inline int parse(const std::string &text, Tuning &t, std::string *problems) {
    t.reset();
    bool pinned[2][5][kLevels] = {}; // values a `.N` line has set: a plain line of the same key later in the file does not overwrite them
    int taken = 0, lineno = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        const std::string raw = text.substr(pos, e - pos);
        pos = e + 1;
        ++lineno;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        auto bad = [&](const std::string &why) {
            if (problems) *problems += "  line " + std::to_string(lineno) + ": " + why + " (ignored): " + line + "\n";
        };
        size_t sp = line.find_first_of(" \t");
        if (sp == std::string::npos) {
            bad("a key and a value are needed");
            continue;
        }
        std::string key = line.substr(0, sp), val = trim(line.substr(sp + 1));
        int level = 0; // 0 = all levels
        const size_t dot = key.find('.');
        if (dot != std::string::npos) {
            const std::string n = key.substr(dot + 1);
            key = key.substr(0, dot);
            char *end = nullptr;
            const long v = std::strtol(n.c_str(), &end, 10);
            if (n.empty() || *end || v < 1 || v > 21) {
                bad("the level after the dot must be 1 to 21");
                continue;
            }
            level = (int)v;
        }
        const KeyDef *def = nullptr;
        for (const KeyDef &d : key_table())
            if (key == d.key) def = &d;
        if (!def) {
            bad("unknown key");
            continue;
        }
        char *end = nullptr;
        const long v = std::strtol(val.c_str(), &end, 10);
        if (val.empty() || *end) {
            bad("the value must be a whole number");
            continue;
        }
        const bool allow_minus1 = def->kind == Kind::LevelCount;
        if (!(v >= def->lo && v <= def->hi) && !(allow_minus1 && v == -1)) {
            bad("the value must be " + std::string(allow_minus1 ? "-1 or " : "") + std::to_string(def->lo) + " to " + std::to_string(def->hi));
            continue;
        }
        if ((def->kind == Kind::Points || def->kind == Kind::ExtraLife) && v % 10 != 0) {
            bad("the value must be a multiple of 10");
            continue;
        }
        if (level != 0 && def->kind != Kind::LevelPercent && def->kind != Kind::LevelCount) {
            bad("this key has no level");
            continue;
        }
        if (def->kind == Kind::Points) t.points[def->slot] = (int)v;
        else if (def->kind == Kind::ExtraLife) t.extra_life_score = (int)v;
        else {
            int *a = level_array(t, def->kind, def->slot);
            const int kind = def->kind == Kind::LevelPercent ? 0 : 1;
            if (level == 0) {
                // the plain key sets every level, but a `.N` line of the same file still wins whatever its place in the file
                for (int l = 1; l < kLevels; ++l)
                    if (!pinned[kind][def->slot][l]) a[l] = (int)v;
            } else a[level] = (int)v, pinned[kind][def->slot][level] = true;
        }
        ++taken;
    }
    t.active = !t.is_standard();
    return taken;
}
inline bool load_file(const std::string &path, Tuning &t, std::string *problems, std::string *error) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    std::fclose(f);
    parse(s, t, problems);
    return true;
}

// The standard rules file: every value as the original game has it, each with a comment line. Modders start from this.
inline std::string standard_file_text() {
    std::string s =
        "# PacArcadeComp rules file (rules.cfg). One comment line, then one `key value` line. Put this file next to PacArcadeComp.exe (or in the settings folder).\n"
        "# While a value differs from the standard one, the game uses the readable rules, the window title says MODDED and the game does not count for records.\n"
        "# Add .N to a level key to change only level N (1 to 21), e.g. `ghost_speed_percent.5 70`. A wrong line is reported in the log and ignored.\n";
    for (const KeyDef &d : key_table()) {
        s += std::string("# ") + d.comment + "\n";
        Tuning std_t;
        std::string v;
        if (d.kind == Kind::Points) v = std::to_string(std_t.points[d.slot]);
        else if (d.kind == Kind::ExtraLife) v = "0";
        else v = std::to_string(d.kind == Kind::LevelPercent ? 100 : -1);
        s += std::string(d.key) + " " + v + "\n";
    }
    return s;
}
inline bool write_standard_file(const std::string &path, std::string *error) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) {
        if (error) *error = "cannot write " + path;
        return false;
    }
    const std::string s = standard_file_text();
    const bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size();
    std::fclose(f);
    if (!ok && error) *error = "write failed";
    return ok;
}

// ---- applying the numbers
// A speed pattern with `percent` percent of the pixel steps (the number of 1 bits scaled, spread evenly). 100 gives the original pattern.
inline uint32_t scale_pattern(uint32_t pattern, int percent) {
    if (percent == 100) return pattern;
    int ones = 0;
    for (int i = 0; i < 32; ++i) ones += (pattern >> i) & 1;
    long k = ((long)ones * percent + 50) / 100;
    if (k > 32) k = 32;
    uint32_t out = 0;
    for (int i = 0; i < 32; ++i)
        if ((long)(i + 1) * k / 32 > (long)i * k / 32) out |= 1u << i;
    return out;
}
inline uint32_t get_pattern(logic::WorkRam ram, unsigned a) { return ((uint32_t)logic::word_at(ram, a) << 16) | logic::word_at(ram, a + 2); }

// Called right after the readable level start (logic::start_level_timers) has filled the memory for level row `row` (0 to 20).
inline void apply_level(logic::WorkRam ram, unsigned row, const Tuning &t) {
    const int level = (int)(row > 20 ? 20 : row) + 1;
    const int pac = t.pacman_speed[level], gh = t.ghost_speed[level];
    if (pac != 100)
        for (int i = 0; i < 2; ++i) logic::set_pattern(ram, logic::kSpeedPatternAddress[i], scale_pattern(get_pattern(ram, logic::kSpeedPatternAddress[i]), pac));
    if (gh != 100) {
        for (int i = 2; i < 7; ++i) logic::set_pattern(ram, logic::kSpeedPatternAddress[i], scale_pattern(get_pattern(ram, logic::kSpeedPatternAddress[i]), gh));
        for (unsigned g = 1; g < 4; ++g)
            for (unsigned i = 0; i < 3; ++i) logic::set_pattern(ram, logic::kSpeedNormalAddress[g] + 4 * i, get_pattern(ram, logic::kSpeedPatternAddress[4 + i]));
    }
    if (t.phase_time[level] != 100)
        for (unsigned i = 0; i < 7; ++i) {
            const uint16_t v = logic::word_at(ram, logic::kPhaseLimitsAddress + 2 * i);
            if (v >= 65534 || v == 0) continue; // "never" stays never
            long n = (long)v * t.phase_time[level] / 100;
            if (n < 1) n = 1;
            if (n > 65533) n = 65533;
            logic::set_word(ram, logic::kPhaseLimitsAddress + 2 * i, (uint16_t)n);
        }
    if (t.frightened_time[level] != 100) {
        long n = (long)logic::word_at(ram, logic::kFrightenedTimeAddress) * t.frightened_time[level] / 100;
        if (n > 65535) n = 65535;
        logic::set_word(ram, logic::kFrightenedTimeAddress, (uint16_t)n);
    }
    if (t.elroy1[level] >= 0) ram[logic::kElroyDotsLeftAddress] = (uint8_t)t.elroy1[level];
    if (t.elroy2[level] >= 0) ram[logic::kElroyDotsLeftAddress + 1] = (uint8_t)t.elroy2[level];
    if (t.pinky_dots[level] >= 0) ram[logic::kHouseLimitsAddress + 0] = (uint8_t)t.pinky_dots[level];
    if (t.inky_dots[level] >= 0) ram[logic::kHouseLimitsAddress + 1] = (uint8_t)t.inky_dots[level];
    if (t.clyde_dots[level] >= 0) ram[logic::kHouseLimitsAddress + 2] = (uint8_t)t.clyde_dots[level];
}

// The scoring of ROM 2A5A with the file's points: same as logic::add_points, but the points and the extra-life score come from the file.
inline logic::ExtraLife add_points_tuned(logic::WorkRam ram, unsigned points_index, const Tuning &t) {
    using namespace logic;
    const unsigned score = kScoreAddress[ram[kCurrentPlayerAddress] != 0];
    const unsigned points = (unsigned)t.points[points_index];
    const uint8_t add[3] = {to_bcd(points % 100), to_bcd((points / 100) % 100), to_bcd((points / 10000) % 100)};
    bool carry = false;
    for (int i = 0; i < 3; ++i) ram[score + i] = add_bcd_byte(ram[score + i], add[i], carry);
    bool reached;
    if (t.extra_life_score > 0) {
        const unsigned now = ((ram[score + 2] >> 4) * 10 + (ram[score + 2] & 15)) * 10000 + ((ram[score + 1] >> 4) * 10 + (ram[score + 1] & 15)) * 100 +
                             (ram[score] >> 4) * 10 + (ram[score] & 15);
        reached = now >= (unsigned)t.extra_life_score;
    } else {
        const uint8_t tt = (uint8_t)((ram[score + 2] << 4) | (ram[score + 1] >> 4));
        reached = (uint8_t)(ram[kBonusLifeLimitAddress] - 1) < tt;
    }
    if (reached) return give_extra_life_once(ram, score + kScoreBytes) ? kExtraLifeGivenNow : kExtraLifeAlreadyGiven;
    return kNoExtraLife;
}

} // namespace rules
