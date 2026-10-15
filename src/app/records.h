// X24: personal bests per level. For each level 1 to 21 (every level from 21 on is kept as "21") the best score the player had when the level was cleared and the
// fastest time the level took (machine frames from the start of the level to the moment its last dot is eaten). A level cleared in a game that was helped
// (assist rule) never changes a record: the caller only passes counted clears. The file records.cfg (plain text, next to controls.cfg) has one line per level:
//   level 3 score 12340 frames 2900
// It lives outside the machine and only reads game events; Purist mode does not update or show it as a result of play.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace records {

constexpr int kLevels = 21;

struct Entry {
    int score = 0;
    unsigned frames = 0;
}; // score 0 / frames 0 = no record yet

struct Result {
    bool first = false, new_score = false, new_time = false;
    int level = 1;
};

inline int bucket(int level) { return level < 1 ? 1 : level > kLevels ? kLevels : level; }

class Table {
public:
    // A level was cleared with `score` points after `frames` machine frames. Returns what changed: first = this level had no record at all.
    Result update(int level, int score, unsigned frames) {
        Result r;
        const int b = bucket(level);
        r.level = b;
        Entry &e = e_[b - 1];
        if (score <= 0 || frames == 0) return r; // nothing sensible to record
        if (e.score == 0 && e.frames == 0) {
            e.score = score;
            e.frames = frames;
            r.first = true;
            return r;
        }
        if (score > e.score) {
            e.score = score;
            r.new_score = true;
        }
        if (frames < e.frames) {
            e.frames = frames;
            r.new_time = true;
        }
        return r;
    }
    const Entry &get(int level) const { return e_[bucket(level) - 1]; }
    bool any() const {
        for (const Entry &e : e_)
            if (e.score || e.frames) return true;
        return false;
    }
    void clear() {
        for (Entry &e : e_) e = Entry();
    }

    bool load(const std::string &path) {
        FILE *f = std::fopen(path.c_str(), "r");
        if (!f) return false;
        char line[200];
        while (std::fgets(line, sizeof line, f)) {
            int lv = 0, sc = 0;
            unsigned fr = 0;
            if (std::sscanf(line, "level %d score %d frames %u", &lv, &sc, &fr) == 3 && lv >= 1 && lv <= kLevels && sc >= 0) {
                e_[lv - 1].score = sc;
                e_[lv - 1].frames = fr;
            }
        }
        std::fclose(f);
        return true;
    }
    // Written to a temporary file first, then moved over the old one (a power cut cannot leave half a file).
    bool save(const std::string &path) const {
        const std::string tmp = path + ".tmp";
        FILE *f = std::fopen(tmp.c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "# Pac-Man port personal bests (one line per level: best score at the end of the level, fastest time in machine frames, 60.606 per second)\n");
        for (int i = 0; i < kLevels; ++i)
            if (e_[i].score || e_[i].frames) std::fprintf(f, "level %d score %d frames %u\n", i + 1, e_[i].score, e_[i].frames);
        if (std::fclose(f) != 0) return false;
        std::remove(path.c_str());
        return std::rename(tmp.c_str(), path.c_str()) == 0;
    }

private:
    Entry e_[kLevels];
};

inline std::string time_text(unsigned frames) { // "0M 45S" (the game's font has no colon and no full stop)
    const unsigned sec = (unsigned)((double)frames * 50688.0 / 3072000.0 + 0.5);
    return std::to_string(sec / 60) + "M " + (sec % 60 < 10 ? "0" : "") + std::to_string(sec % 60) + "S";
}

// One line of the Records page (at most 24 characters): "01 12340  0M 45S", "--" when there is no record.
inline std::string line(int level, const Entry &e) {
    char b[40];
    std::snprintf(b, sizeof b, level == kLevels ? "21+" : "%02d ", level);
    std::string s = level == kLevels ? "21+ " : std::string(b);
    if (!e.score && !e.frames) return s + "-";
    std::string sc = std::to_string(e.score);
    while (sc.size() < 7) sc = " " + sc;
    return s + sc + "  " + time_text(e.frames);
}

} // namespace records
