// menu.cpp: drawing and handling of the pause menu declared in menu.h (compiled as part of the program in main.cpp). It only returns commands.
#include "menu.h"
#include "crt.h" // X43: the CRT look presets
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include "overlay.h"

namespace menu {

// ---- pages: 0 main, 1 game options, 2 controls, 3 display. An item is a command, or a sub page (Cmd::None with sub >= 0), or "back" (Cmd::None, sub = -1).
struct Item {
    const char *label;
    Cmd cmd;
    int sub;
};
static const Item kMain[] = {{"RESUME", Cmd::Resume, -1},        {"SAVE STATE F2", Cmd::SaveState, 4}, {"LOAD STATE F4", Cmd::LoadState, 5},
                             {"GAME OPTIONS", Cmd::None, 1},     {"CONTROLS", Cmd::None, 2},           {"DISPLAY", Cmd::None, 3},
                             {"RECORDS", Cmd::None, 6},          {"ACHIEVEMENTS", Cmd::None, 7},       {"PURIST MODE", Cmd::Purist, -1},
                             {"RESTART GAME", Cmd::Restart, -1}, {"QUIT GAME", Cmd::Quit, -1}};
static const Item kGame[] = {{"LIVES", Cmd::Lives, -1},       {"BONUS LIFE", Cmd::Bonus, -1},        {"DIFFICULTY", Cmd::Difficulty, -1},
                             {"GHOST NAMES", Cmd::Names, -1}, {"APPLY - RESTART", Cmd::Restart, -1}, {"RULES", Cmd::Rules, -1},
                             {"WRITE RULES FILE", Cmd::WriteRules, -1}, {"BACK", Cmd::None, -1}};
static const Item kControls[] = {{"BINDINGS", Cmd::Bindings, -1},
                                 {"CHANGE KEYS F9", Cmd::Keys, -1},
                                 {"CHANGE PAD F10", Cmd::Pad, -1},
                                 {"RESET KEYS F12", Cmd::ResetControls, -1},
                                 {"ONE BUTTON START", Cmd::OneButton, -1},
                                 {"VIBRATION", Cmd::Vibration, -1},
                                 {"PAD SYMBOLS", Cmd::PadSymbols, -1},
                                 {"FOCUS LOSS PAUSE", Cmd::FocusPause, -1},
                                 {"HINTS", Cmd::Hints, -1},
                                 {"LIGHTBAR", Cmd::Lightbar, -1},
                                 {"BACK", Cmd::None, -1}};
static const Item kDisplay[] = {{"OVERLAY F3", Cmd::Overlay, -1},
                                {"SCANLINES F5", Cmd::Scanlines, -1},
                                {"SMOOTH PACING F6", Cmd::Pacing, -1},
                                {"WINDOW FRAME F7", Cmd::Frame, -1},
                                {"SCALING F8", Cmd::Scaling, -1},
                                {"FULL SCREEN F11", Cmd::FullScreen, -1},
                                {"HELP PANEL", Cmd::HelpPin, -1},
                                {"RUN SUMMARY", Cmd::RunSummary, -1},
                                {"CRT LOOK", Cmd::CrtLook, -1},
                                {"CRT MASK", Cmd::CrtMask, -1},
                                {"GLOW", Cmd::Glow, -1},
                                {"CURVE", Cmd::Curve, -1},
                                {"SURROUND", Cmd::Surround, -1},
                                {"BACK", Cmd::None, -1}};
static const Item kSaveSlots[] = {{"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1},
                                  {"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1}, {"", Cmd::SaveSlot, -1}, {"BACK", Cmd::None, -1}};
static const Item kLoadSlots[] = {{"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1},
                                  {"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1}, {"", Cmd::LoadSlot, -1}, {"BACK", Cmd::None, -1}};
static const Item kRecords[] = {{"BACK", Cmd::None, -1}}; // X24: page 6 is drawn as a table of 21 levels (Menu::draw); Enter or Esc goes back
static const Item *ach_items(int *n) {                    // X25: page 7: one row per achievement (the name) and BACK
    static Item it[achievements::kCount + 1];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < achievements::kCount; ++i) it[i] = {achievements::def(i).name, Cmd::None, -2};
        it[achievements::kCount] = {"BACK", Cmd::None, -1};
        init = true;
    }
    *n = achievements::kCount + 1;
    return it;
}
static const Item *items_of(int page, int *n) {
    switch (page) {
    case 6: *n = 1; return kRecords;
    case 7: return ach_items(n);
    case 4: *n = (int)(sizeof kSaveSlots / sizeof *kSaveSlots); return kSaveSlots;
    case 5: *n = (int)(sizeof kLoadSlots / sizeof *kLoadSlots); return kLoadSlots;
    case 1: *n = (int)(sizeof kGame / sizeof *kGame); return kGame;
    case 2: *n = (int)(sizeof kControls / sizeof *kControls); return kControls;
    case 3: *n = (int)(sizeof kDisplay / sizeof *kDisplay); return kDisplay;
    default: *n = (int)(sizeof kMain / sizeof *kMain); return kMain;
    }
}
static const char *kPageTitle[] = {"PAUSED", "GAME OPTIONS", "CONTROLS", "DISPLAY", "SAVE STATE", "LOAD STATE", "RECORDS", "ACHIEVEMENTS"};

bool locked(Cmd c, const Model &m) {
    if (!m.purist) return false;
    return c == Cmd::SaveState || c == Cmd::LoadState || c == Cmd::SaveSlot || c == Cmd::LoadSlot || c == Cmd::OneButton || c == Cmd::Vibration || c == Cmd::FocusPause ||
           c == Cmd::Hints || c == Cmd::Lightbar || c == Cmd::RunSummary || c == Cmd::CrtMask || c == Cmd::Glow || c == Cmd::Curve || c == Cmd::CrtLook ||
           c == Cmd::Surround || c == Cmd::Overlay || c == Cmd::Scanlines || c == Cmd::Pacing || c == Cmd::Scaling || c == Cmd::Rules;
}
std::string label_of(Cmd c) {
    for (int p = 0; p < 8; ++p) {
        int n;
        const Item *it = items_of(p, &n);
        for (int i = 0; i < n; ++i)
            if (it[i].cmd == c && it[i].label[0]) return it[i].label;
    }
    return "";
}
std::string value_text(Cmd c, const Model &m) {
    static const char *lives[] = {"1", "2", "3", "5"}, *bonus[] = {"10000", "15000", "20000", "NONE"};
    auto onoff = [](bool b) { return std::string(b ? "ON" : "OFF"); };
    switch (c) {
    case Cmd::Lives: return lives[m.lives & 3];
    case Cmd::Bonus: return bonus[m.bonus & 3];
    case Cmd::Difficulty: return m.diff_normal ? "NORMAL" : "HARD";
    case Cmd::Names: return m.names_normal ? "NORMAL" : "ALTERNATE";
    case Cmd::Rules: return m.purist ? "ORIGINAL" : m.rules_file ? "FILE" : m.rules_readable ? "READABLE" : "ORIGINAL";
    case Cmd::OneButton: return onoff(m.one_button && !m.purist);
    case Cmd::PadSymbols: return m.pad_symbols == 1 ? "XBOX" : m.pad_symbols == 2 ? "PS4" : m.pad_symbols == 3 ? "PS5" : "AUTO";
    case Cmd::Vibration: return m.purist || m.vibration == 0 ? "OFF" : m.vibration == 1 ? "LIGHT" : "FULL";
    case Cmd::FocusPause: return onoff(m.pause_focus && !m.purist);
    case Cmd::Lightbar: return onoff(m.lightbar && !m.purist);
    case Cmd::RunSummary: return onoff(m.run_summary && !m.purist);
    case Cmd::Surround: return m.purist || m.surround == 0 ? "BLACK" : m.surround == 2 ? "AMBIENT" : "CABINET";
    case Cmd::CrtLook: return m.purist ? "OFF" : crt::look_name(m.crt_mask, m.glow, m.curve, m.scanlines);
    case Cmd::Curve: return m.purist || m.curve == 0 ? "OFF" : m.curve == 1 ? "LIGHT" : "STRONG";
    case Cmd::Glow: return m.purist || m.glow == 0 ? "OFF" : m.glow == 1 ? "LIGHT" : "STRONG";
    case Cmd::CrtMask: return m.purist || m.crt_mask == 0 ? "OFF" : m.crt_mask == 1 ? "LIGHT" : "STRONG";
    case Cmd::Hints: return m.hints == 1 ? "KEYBOARD" : m.hints == 2 ? "PAD" : "AUTO";
    case Cmd::Overlay: return onoff(m.overlay && !m.purist);
    case Cmd::Scanlines: return onoff(m.scanlines && !m.purist);
    case Cmd::Pacing: return (m.smooth && !m.purist) ? "SMOOTH" : "EXACT";
    case Cmd::Frame: return onoff(m.frame);
    case Cmd::Scaling: return (m.fill && !m.purist) ? "FILL" : "WHOLE";
    case Cmd::FullScreen: return onoff(m.fullscreen);
    case Cmd::HelpPin: return onoff(m.help_pin);
    case Cmd::Purist: return onoff(m.purist);
    case Cmd::Resume: return m.pad_name[0];
    case Cmd::SaveState: return m.pad_name[2];
    case Cmd::LoadState: return m.pad_name[3];
    default: return "";
    }
}

static bool is_choice(Cmd c) {
    return c == Cmd::Lives || c == Cmd::Bonus || c == Cmd::Difficulty || c == Cmd::Names || c == Cmd::OneButton || c == Cmd::Vibration || c == Cmd::PadSymbols ||
           c == Cmd::FocusPause || c == Cmd::Hints || c == Cmd::Lightbar || c == Cmd::RunSummary || c == Cmd::CrtMask || c == Cmd::Glow || c == Cmd::Curve ||
           c == Cmd::CrtLook || c == Cmd::Surround || c == Cmd::Overlay || c == Cmd::Scanlines || c == Cmd::Pacing || c == Cmd::Frame || c == Cmd::Scaling ||
           c == Cmd::FullScreen || c == Cmd::HelpPin || c == Cmd::Purist || c == Cmd::Rules;
}
// The menu box as Menu::draw() puts it (keep these numbers in step with draw()).
static const int kRowH = 14, kBoxW = 208, kBoxX = 8, kBoxY = 20, kRow0 = 28;

int Menu::item_at(int px, int py, bool *right_half) const {
    if (page_ == 6 || page_ == 7) return -1; // X24 / X25: these pages are drawn as tables, the mouse does not pick rows there
    int n;
    items_of(page_, &n);
    if (px < kBoxX + 4 || px >= kBoxX + kBoxW - 4) return -1;
    int rel = py - (kBoxY + kRow0 - 2); // the highlight bar of row i starts 2 pixels above its text
    if (rel < 0 || rel >= n * kRowH) return -1;
    if (right_half) *right_half = px >= kBoxX + kBoxW / 2;
    return rel / kRowH;
}
bool Menu::hover(int px, int py) {
    int i = item_at(px, py);
    if (i < 0 || i == cur_[page_]) return false;
    cur_[page_] = i;
    return true;
}
Result Menu::click(int px, int py, const Model &m) {
    bool right = false;
    int i = item_at(px, py, &right);
    Result r;
    if (i < 0) return r;
    int n;
    const Item *it = items_of(page_, &n);
    cur_[page_] = i;
    if (is_choice(it[i].cmd) && it[i].sub < 0) return key(right ? Key::Right : Key::Left, m);
    return key(Key::Enter, m);
}

Result Menu::key(Key k, const Model &m) {
    int n;
    const Item *it = items_of(page_, &n);
    int &c = cur_[page_];
    Result r;
    if (k == Key::Up) {
        c = (c + n - 1) % n;
        return r;
    }
    if (k == Key::Down) {
        c = (c + 1) % n;
        return r;
    }
    if (k == Key::Back) {
        if (page_ == 0) {
            r.cmd = Cmd::Resume;
            r.delta = 1;
        } else page_ = 0;
        return r;
    }
    const Item &i = it[c];
    if (i.sub >= 0) {
        if (k != Key::Left) {
            if (locked(i.cmd, m)) {
                r.cmd = Cmd::Locked;
                return r;
            }
            page_ = i.sub;
            cur_[page_] = 0;
        }
        return r;
    } // (a sub page item may carry a command: SAVE STATE / LOAD STATE are locked in Purist)
    if (i.cmd == Cmd::SaveSlot || i.cmd == Cmd::LoadSlot) {
        if (k == Key::Left) return r;
        if (locked(i.cmd, m)) {
            r.cmd = Cmd::Locked;
            return r;
        }
        r.cmd = i.cmd;
        r.delta = c + 1;
        return r;
    }
    if (i.sub == -2) return r; // X25: an achievement row: nothing to do (the description shows below)
    if (i.cmd == Cmd::None) {
        page_ = 0;
        return r;
    } // BACK
    if (locked(i.cmd, m)) {
        r.cmd = Cmd::Locked;
        r.delta = 0;
        return r;
    }
    bool choice = is_choice(i.cmd);
    if (k == Key::Left) {
        if (!choice) return r;
        r.cmd = i.cmd;
        r.delta = -1;
        return r;
    }
    r.cmd = i.cmd;
    r.delta = 1;
    return r;
}

std::vector<std::string> Menu::visible_lines(const Model &m) const {
    std::vector<std::string> v;
    int n;
    const Item *it = items_of(page_, &n);
    for (int i = 0; i < n; ++i) {
        std::string s = it[i].label, val = value_text(it[i].cmd, m);
        if ((it[i].cmd == Cmd::SaveSlot || it[i].cmd == Cmd::LoadSlot) && i < 8) {
            s = m.slot_line[i].empty() ? std::to_string(i + 1) + " EMPTY" : m.slot_line[i];
            val.clear();
        }
        if (locked(it[i].cmd, m)) val = "LOCKED";
        if (!val.empty()) {
            while (s.size() + val.size() < 24) s += ' ';
            s += val;
        }
        v.push_back(s);
    }
    return v;
}

void Menu::draw(const Gfx &g, uint32_t *out, const Model &m) const {
    using namespace overlay;
    for (int y = 0; y < kPictureH; ++y)
        for (int x = 0; x < kPictureW; ++x) {
            uint32_t &p = out[y * kOutW + x];
            p = (p >> 1) & 0x7F7F7F;
        }
    if (page_ == 6) { // X24: the records table: 21 levels, best score and fastest time
        const int bx6 = kBoxX, by6 = 4, bw6 = kBoxW, bh6 = 262;
        rect(out, bx6, by6, bw6, bh6, 0x000028);
        frame_box(out, bx6, by6, bw6, bh6, 0xFFFF00);
        text(g, out, bx6 + 8, by6 + 8, kPageTitle[6], 0xFFFFFF);
        if (m.pad_type != PadType::None) draw_pad_icon(out, bx6 + bw6 - 8 - 16, by6 + 7, m.pad_type);
        text(g, out, bx6 + 8, by6 + 22, "LV    SCORE      TIME", 0x00FFFF);
        for (int i = 0; i < 21; ++i) {
            const std::string &l = m.rec_line[i];
            text(g, out, bx6 + 8, by6 + 34 + 9 * i, (l.empty() ? std::string(i == 20 ? "21+ -" : (i < 9 ? "0" : "") + std::to_string(i + 1) + " -") : l).c_str(),
                 l.empty() || l.back() == '-' ? 0x808080 : 0xFFFFFF);
        }
        rect(out, bx6 + 4, by6 + 34 + 9 * 21 + 3, bw6 - 8, 11, 0x303090);
        text(g, out, bx6 + 8, by6 + 34 + 9 * 21 + 5, "BACK", 0xFFFF00);
        text(g, out, bx6 + 8, by6 + bh6 - 16, note.empty() ? "ESC GOES BACK" : note.c_str(), 0x909090);
        return;
    }
    if (page_ == 7) { // X25: the achievements list (scrolls with the cursor), the description of the highlighted one, the rule
        int na;
        const Item *ai = items_of(7, &na);
        const int total = achievements::kCount;
        const int bx7 = kBoxX, by7 = 4, bw7 = kBoxW, bh7 = 262, vis = 10, cur = cur_[7];
        rect(out, bx7, by7, bw7, bh7, 0x000028);
        frame_box(out, bx7, by7, bw7, bh7, 0xFFFF00);
        text(g, out, bx7 + 8, by7 + 8, kPageTitle[7], 0xFFFFFF);
        if (m.pad_type != PadType::None) draw_pad_icon(out, bx7 + bw7 - 8 - 16, by7 + 7, m.pad_type);
        int done = 0;
        for (int i = 0; i < total; ++i) done += (int)((m.ach_mask >> i) & 1ULL);
        text(g, out, bx7 + 8, by7 + 22, (std::to_string(done) + " OF " + std::to_string(total) + " UNLOCKED").c_str(), 0x00FFFF);
        int first = cur - vis / 2;
        if (first > na - vis) first = na - vis;
        if (first < 0) first = 0;
        for (int r = 0; r < vis && first + r < na; ++r) {
            const int i = first + r, y = by7 + 36 + 14 * r;
            const bool sel = i == cur;
            if (sel) rect(out, bx7 + 4, y - 2, bw7 - 8, 11, 0x303090);
            if (i < total) {
                const bool got = (m.ach_mask >> i) & 1ULL;
                std::string nm = ai[i].label, st = got ? "DONE" : "LOCKED";
                while (nm.size() + st.size() < 24) nm += ' ';
                text(g, out, bx7 + 8, y, (nm + st).c_str(), got ? (sel ? 0x80FF80 : 0x00C000) : sel ? 0xFFFF00 : 0x909090);
            } else text(g, out, bx7 + 8, y, "BACK", sel ? 0xFFFF00 : 0xFFFFFF);
        }
        if (first > 0) text(g, out, bx7 + bw7 - 16, by7 + 22, "^", 0x909090);
        if (first + vis < na) text(g, out, bx7 + bw7 - 16, by7 + 36 + 14 * vis - 14, "v", 0x909090);
        if (cur < total) { // the description: wrapped at 24 characters
            std::string d = achievements::def(cur).text;
            int y = by7 + 36 + 14 * vis + 4;
            size_t p0 = 0;
            while (p0 < d.size() && y < by7 + bh7 - 58) {
                size_t e = std::min(d.size(), p0 + 24);
                if (e < d.size()) {
                    size_t sp = d.rfind(' ', e);
                    if (sp != std::string::npos && sp > p0) e = sp;
                }
                text(g, out, bx7 + 8, y, d.substr(p0, e - p0).c_str(), 0xFFFFFF);
                y += 10;
                p0 = e;
                while (p0 < d.size() && d[p0] == ' ') ++p0;
            }
            const bool got = (m.ach_mask >> cur) & 1ULL; // X29: progress line (only for achievements with a count that are not unlocked yet)
            if (!got && cur < 24 && m.ach_need[cur] > 0) {
                const int py = by7 + bh7 - 52, hv = m.ach_have[cur], nd = m.ach_need[cur];
                text(g, out, bx7 + 8, py, ("PROGRESS " + std::to_string(hv) + " OF " + std::to_string(nd)).c_str(), 0x00FFFF);
                const int bw = 64, bxp = bx7 + bw7 - 8 - bw;
                rect(out, bxp, py, bw, 8, 0x303090);
                rect(out, bxp + 1, py + 1, (bw - 2) * hv / nd, 6, 0x00C0C0);
            }
        }
        text(g, out, bx7 + 8, by7 + bh7 - 42, achievements::kRule1, 0xFF8040);
        text(g, out, bx7 + 8, by7 + bh7 - 32, achievements::kRule2, 0xFF8040);
        text(g, out, bx7 + 8, by7 + bh7 - 18, note.empty() ? "ESC GOES BACK" : note.c_str(), 0x909090);
        return;
    }
    int n;
    const Item *it = items_of(page_, &n);
    const int rowh = kRowH, boxw = kBoxW, boxh = 40 + n * rowh + 26, bx = kBoxX, by = kBoxY;
    rect(out, bx, by, boxw, boxh, 0x000028);
    frame_box(out, bx, by, boxw, boxh, 0xFFFF00);
    text(g, out, bx + 8, by + 8, kPageTitle[page_], 0xFFFFFF);
    if (m.pad_type != PadType::None) draw_pad_icon(out, bx + boxw - 8 - 16, by + 7, m.pad_type); // S34C: the controller in use
    if (m.purist) text(g, out, bx + boxw - 8 - 16 - 4 - 8 * 6, by + 8, "PURIST", 0x00FF00);
    std::vector<std::string> lines = visible_lines(m);
    for (int i = 0; i < n; ++i) {
        int y = by + kRow0 + i * rowh;
        bool sel = i == cur_[page_];
        bool lk = locked(it[i].cmd, m);
        if (sel) rect(out, bx + 4, y - 2, boxw - 8, 11, 0x303090);
        text(g, out, bx + 8, y, lines[i].c_str(), lk ? 0x808080 : sel ? 0xFFFF00 : 0xFFFFFF);
    }
    int fy = by + boxh - 24;
    if (!note.empty()) text(g, out, bx + 8, fy, note.c_str(), 0x00FFFF);
    else if (page_ == 1 && m.pending) text(g, out, bx + 8, fy, "RESTART NEEDED", 0xFF8040);
    else if (!m.assist.empty()) text(g, out, bx + 8, fy, ("ASSISTED " + m.assist).substr(0, 24).c_str(), 0xFF8040); // X03
    else text(g, out, bx + 8, fy, page_ == 0 ? "ESC RESUMES" : "ESC GOES BACK", 0x909090);
    if (hints_are_pad(m)) { // X08: the hints follow the hand: pad pictures ...
        int hx = bx + 8;
        hx += keycap_width("DPAD") + 4;
        draw_keycap(g, out, bx + 8, fy + 10, "DPAD");
        hx += draw_glyph(g, out, hx, fy + 11, m.pad_type, 0, 40) + 3;
        text(g, out, hx, fy + 11, "OK", 0x909090);
        hx += 16 + 6;
        hx += draw_glyph(g, out, hx, fy + 11, m.pad_type, 1, 40) + 3;
        text(g, out, hx, fy + 11, "BACK", 0x909090);
    } else { // ... or key caps
        int hx = bx + 8;
        hx += draw_keycap(g, out, hx, fy + 10, "UP") + 2;
        hx += draw_keycap(g, out, hx, fy + 10, "DOWN") + 2;
        hx += draw_keycap(g, out, hx, fy + 10, "ENTER") + 2;
        draw_keycap(g, out, hx, fy + 10, "ESC");
    }
    if (m.pad_type != PadType::None) {
        int w = 8 * (int)std::strlen(pad_type_word(m.pad_type));
        text(g, out, bx + boxw - 8 - w, fy + 11, pad_type_word(m.pad_type), 0x00FF00);
    }
}

void draw_focus_pause(const Gfx &g, uint32_t *out) {
    using namespace overlay;
    for (int y = 0; y < kPictureH; ++y)
        for (int x = 0; x < kPictureW; ++x) {
            uint32_t &p = out[y * kOutW + x];
            p = (p >> 1) & 0x7F7F7F;
        }
    const int bw = 160, bh = 44, bx = (kPictureW - bw) / 2, by = (kPictureH - bh) / 2;
    rect(out, bx, by, bw, bh, 0x000028);
    frame_box(out, bx, by, bw, bh, 0xFFFF00);
    text(g, out, bx + (bw - 8 * 6) / 2, by + 10, "PAUSED", 0xFFFFFF);
    text(g, out, bx + (bw - 8 * 13) / 2, by + 26, "PRESS ANY KEY", 0xFFFF00);
}

void draw_achievement_popup(const Gfx &g, uint32_t *out, int id) {
    using namespace overlay;
    const int bw = 208, bh = 30, bx = (kPictureW - bw) / 2, by = 26;
    rect(out, bx, by, bw, bh, 0x000028);
    frame_box(out, bx, by, bw, bh, 0x00C000);
    const char *head = "ACHIEVEMENT UNLOCKED";
    const std::string nm = achievements::def(id).name;
    text(g, out, bx + (bw - 8 * (int)std::strlen(head)) / 2, by + 5, head, 0x00FF00);
    text(g, out, bx + (bw - 8 * (int)nm.size()) / 2, by + 17, nm.c_str(), 0xFFFFFF);
}

void draw_new_best(const Gfx &g, uint32_t *out, const std::string &line1, const std::string &line2) {
    using namespace overlay;
    const int bw = 192, bh = 34, bx = (kPictureW - bw) / 2, by = 120;
    rect(out, bx, by, bw, bh, 0x000028);
    frame_box(out, bx, by, bw, bh, 0xFFFF00);
    text(g, out, bx + (bw - 8 * (int)line1.size()) / 2, by + 6, line1.c_str(), 0xFFFF00);
    text(g, out, bx + (bw - 8 * (int)line2.size()) / 2, by + 20, line2.c_str(), 0xFFFFFF);
}

void draw_run_summary(const Gfx &g, uint32_t *out, const stats::Summary &s) {
    using namespace overlay;
    const int bw = 176, bh = 132, bx = (kPictureW - bw) / 2, by = (kPictureH - bh) / 2;
    rect(out, bx, by, bw, bh, 0x000028);
    frame_box(out, bx, by, bw, bh, 0xFFFF00);
    const std::string title = s.two_players ? std::string("PLAYER ") + std::to_string(s.player) + " SUMMARY" : "GAME SUMMARY";
    text(g, out, bx + (bw - 8 * (int)title.size()) / 2, by + 10, title.c_str(), 0xFFFFFF);
    if (!s.run.counted) {
        text(g, out, bx + (bw - 8 * 13) / 2, by + 42, "ASSISTED GAME", 0xFF8000);
        text(g, out, bx + (bw - 8 * 11) / 2, by + 58, "NOT COUNTED", 0xFF8000);
    } else {
        const std::string v[5] = {std::to_string(s.run.score), std::to_string(s.run.level), std::to_string(s.run.ghosts), std::to_string(s.run.fruit),
                                  stats::time_text(s.run.seconds())};
        static const char *lab[5] = {"SCORE", "LEVEL", "GHOSTS", "FRUIT", "TIME"};
        for (int i = 0; i < 5; ++i) {
            const int y = by + 32 + 16 * i;
            text(g, out, bx + 12, y, lab[i], 0x00FFFF);
            text(g, out, bx + bw - 12 - 8 * (int)v[i].size(), y, v[i].c_str(), 0xFFFFFF);
        }
    }
    text(g, out, bx + (bw - 8 * 13) / 2, by + bh - 20, "PRESS ANY KEY", 0xFFFF00);
}

int keycap_width(const std::string &label) {
    if (label == "UP" || label == "DOWN" || label == "LEFT" || label == "RIGHT") return 15;
    return 8 * (int)label.size() + 6;
}
int draw_keycap(const Gfx &g, uint32_t *out, int x, int y, const std::string &label) {
    using namespace overlay;
    const int w = keycap_width(label), h = 10;
    rect(out, x, y, w, h, 0x7A7A92);
    rect(out, x + 1, y + 1, w - 2, h - 3, 0x3A3A52);
    rect(out, x + 1, y + h - 2, w - 2, 1, 0x20203A); // the cap's top, face and shadow
    const bool arrow = label == "UP" || label == "DOWN" || label == "LEFT" || label == "RIGHT";
    if (!arrow) {
        text(g, out, x + 3, y + 1, label.c_str(), 0xFFFFFF);
        return w;
    }
    const int cx = x + w / 2, cy = y + 4; // a small triangle, 4 rows / 4 columns
    for (int i = 0; i < 4; ++i) {
        if (label == "UP") rect(out, cx - i, cy - 2 + i, 2 * i + 1, 1, 0xFFFFFF);
        else if (label == "DOWN") rect(out, cx - (3 - i), cy - 2 + i, 2 * (3 - i) + 1, 1, 0xFFFFFF);
        else if (label == "LEFT") rect(out, cx - 2 + i, cy - i, 1, 2 * i + 1, 0xFFFFFF);
        else rect(out, cx - 2 + i, cy - (3 - i), 1, 2 * (3 - i) + 1, 0xFFFFFF);
    }
    return w;
}
bool hints_are_pad(const Model &m) {
    if (m.purist) return false;
    return lastdevice::hints_use_pad(m.hints, m.hint_device == 3 ? lastdevice::Device::Pad : lastdevice::Device::Keyboard, m.pad_type != PadType::None);
}

void draw_help(const Gfx &g, uint32_t *out, const Model &m) {
    using namespace overlay;
    for (int y = 0; y < kOutH; ++y)
        for (int x = kPictureW; x < kOutW; ++x) out[y * kOutW + x] = 0x101018;
    struct L {
        const char *key;
        const char *act;
        bool lock;
        int pad;
    }; // pad: index into Model::pad_btn / pad_name, or -1
    const L lines[] = {{"ESC", "MENU", false, 0},      {"F1", "PAUSE", false, 1},       {"F2", "SAVE", true, 2},       {"F4", "LOAD", true, 3},
                       {"BKSP", "REWIND", true, 4},    {"F3", "OVERLAY", true, -1},     {"F5", "SCANLINES", true, -1}, {"F6", "PACING", true, -1},
                       {"F7", "FRAME", false, -1},     {"F8", "SCALING", true, -1},     {"F9", "KEYS", false, -1},     {"F10", "PAD", false, -1},
                       {"F11", "FULLSCRN", false, -1}, {"F12", "RESET KEYS", false, -1}};
    const bool padset = hints_are_pad(m);
    int x = kPictureW + 6, y = 8;
    text(g, out, x, y, "KEYS", 0xFFFFFF);
    text(g, out, x + 36, y, "PIN", m.help_pin ? 0x00FF00 : 0x606060); // X05: click the header to pin / unpin this panel (green = pinned)
    if (m.pad_type != PadType::None) {
        int w = 8 * (int)std::strlen(pad_type_word(m.pad_type));
        text(g, out, kOutW - 6 - w, y, pad_type_word(m.pad_type), 0x00FF00);
        draw_pad_icon(out, kOutW - 6 - w - 20, y - 1, m.pad_type);
    }
    y += 12;
    for (const L &l : lines) {
        const uint32_t col = (m.purist && l.lock) ? 0x606060 : 0xDDDDDD;
        if (padset) { // X08: the pad's pictures (only the commands that have a pad button)
            if (l.pad < 0) continue;
            int gx = x;
            if (m.pad_btn[l.pad] >= 0) gx += draw_glyph(g, out, gx, y, m.pad_type, m.pad_btn[l.pad], 60) + 4;
            else gx += draw_glyph(g, out, gx, y, m.pad_type, -1, 8) + 4;
            text(g, out, gx, y, l.act, col);
        } else { // key caps
            int cx = x + draw_keycap(g, out, x, y - 1, l.key) + 4;
            text(g, out, cx, y, l.act, col);
        }
        y += 11;
    }
    if (padset) {
        text(g, out, x, y, "MORE: MENU", 0x808080);
        y += 11;
    }
    y += 6;
    text(g, out, x, y, "YOUR CONTROLS", 0xFFFFFF);
    y += 12;
    if (padset) {
        text(g, out, x, y, "MOVE", 0xFFFF00);
        text(g, out, x + 8 * 7, y, "DPAD", 0xFFFF00);
        y += 11;
        static const char *nm[3] = {"COIN", "START1", "START2"};
        for (int i = 0; i < 3; ++i) {
            text(g, out, x, y, nm[i], 0xFFFF00);
            draw_glyph(g, out, x + 8 * 7, y, m.pad_type, m.pad_game_btn[i], kOutW - 2 - (x + 8 * 7));
            y += 11;
        }
    } else {
        static const char *nm[7] = {"UP", "DOWN", "LEFT", "RIGHT", "COIN", "START1", "START2"};
        for (int i = 0; i < 7; ++i) {
            if (m.key_cap[i][0].empty() && m.key_cap[i][1].empty()) {
                if (m.bind_line[i].empty()) continue;
                text(g, out, x, y, m.bind_line[i].c_str(), 0xFFFF00);
                y += 11;
                continue;
            }
            text(g, out, x, y, nm[i], 0xFFFF00);
            int cx = x + 8 * 7;
            for (int k = 0; k < 2; ++k)
                if (!m.key_cap[i][k].empty() && cx + keycap_width(m.key_cap[i][k]) < kOutW - 2) cx += draw_keycap(g, out, cx, y - 1, m.key_cap[i][k]) + 2;
            y += 11;
        }
    }
    y += 6;
    if (m.purist) {
        text(g, out, x, y, "PURIST ON", 0x00FF00);
        y += 11;
        text(g, out, x, y, "GREY KEYS", 0x808080);
        y += 11;
        text(g, out, x, y, "ARE OFF", 0x808080);
    }
}

// ---- S34A: names and the bindings screen
std::string display_key(int sc) {
    if (sc <= 0) return "-";
    std::string n = controls::key_name(sc), u;
    if (n == "none") return "-";
    static const char *const sym[][2] = {{",", "COMMA"},      {".", "PERIOD"},       {";", "SEMICOLON"},     {"'", "APOSTROPHE"}, {"/", "SLASH"},
                                         {"\\", "BACKSLASH"}, {"[", "LEFT BRACKET"}, {"]", "RIGHT BRACKET"}, {"=", "EQUALS"},     {"`", "GRAVE"}};
    for (const auto &e : sym)
        if (n == e[0]) return e[1];
    for (char ch : n) u += (char)((ch >= 'a' && ch <= 'z') ? ch - 32 : ch);
    return u;
}
std::string display_pad(int b) {
    if (b < 0) return "-";
    std::string n = controls::pad_name(b), u;
    for (char ch : n) u += (char)((ch >= 'a' && ch <= 'z') ? ch - 32 : ch);
    static const char *const nm[][2] = {{"LEFTTRIGGER", "L2/LT"}, {"RIGHTTRIGGER", "R2/RT"},   {"MISC1", "SHARE/MIC"},      {"TOUCHPAD", "TOUCHPAD"},
                                        {"PADDLE1", "PADDLE 1"},  {"PADDLE2", "PADDLE 2"},     {"PADDLE3", "PADDLE 3"},     {"PADDLE4", "PADDLE 4"},
                                        {"LEFTSHOULDER", "LB"},   {"RIGHTSHOULDER", "RB"},     {"LEFTSTICK", "L3"},         {"RIGHTSTICK", "R3"},
                                        {"DPAD_UP", "D-PAD UP"},  {"DPAD_DOWN", "D-PAD DOWN"}, {"DPAD_LEFT", "D-PAD LEFT"}, {"DPAD_RIGHT", "D-PAD RIGHT"}};
    for (const auto &e : nm)
        if (u == e[0]) return e[1];
    return u;
}
static std::string clip(const std::string &s, size_t n) { return s.size() <= n ? s : s.substr(0, n - 1) + "-"; }
// ---- S34C: pad type, the icon and the button pictures (all drawn in code: no image files, nothing copied)
PadType pad_type_of_sdl(int t, bool standard_pad) {
    if (!standard_pad) return PadType::Other;
    return t == 1 || t == 2 ? PadType::Xbox : t == 3 || t == 4 ? PadType::PS4 : t == 7 ? PadType::PS5 : PadType::Other;
}
const char *pad_type_word(PadType t) { return t == PadType::Xbox ? "XBOX" : t == PadType::PS4 ? "PS4" : t == PadType::PS5 ? "PS5" : t == PadType::Other ? "PAD" : ""; }

static void px(uint32_t *out, int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < overlay::kOutW && y < overlay::kOutH) out[y * overlay::kOutW + x] = c;
}
static void disc(uint32_t *out, int cx2, int cy2, int r2, uint32_t c) { // coordinates doubled so that even sizes are centred: a disc of diameter r2
    for (int y = 0; y < r2; ++y)
        for (int x = 0; x < r2; ++x) {
            int dx = 2 * x + 1 - r2, dy = 2 * y + 1 - r2;
            if (dx * dx + dy * dy <= r2 * r2) px(out, cx2 + x, cy2 + y, c);
        }
}
// 9 x 9 shapes of the PlayStation face buttons
static const char *const kCross[9] = {"X.......X", ".X.....X.", "..X...X..", "...X.X...", "....X....", "...X.X...", "..X...X..", ".X.....X.", "X.......X"};
static const char *const kCircle[9] = {"..XXXXX..", ".X.....X.", "X.......X", "X.......X", "X.......X", "X.......X", "X.......X", ".X.....X.", "..XXXXX.."};
static const char *const kSquare[9] = {"XXXXXXXXX", "X.......X", "X.......X", "X.......X", "X.......X", "X.......X", "X.......X", "X.......X", "XXXXXXXXX"};
static const char *const kTriangle[9] = {"....X....", "....X....", "...X.X...", "...X.X...", "..X...X..", "..X...X..", ".X.....X.", ".X.....X.", "XXXXXXXXX"};
static void shape(uint32_t *out, int x, int y, const char *const rows[9], uint32_t c) {
    for (int j = 0; j < 9; ++j)
        for (int i = 0; i < 9; ++i)
            if (rows[j][i] == 'X') {
                px(out, x + i, y + j, c);
                if (i + 1 < 9 && rows[j][i + 1] != 'X') px(out, x + i + 1, y + j, c);
            } // a pixel thicker
}
std::string glyph_label(PadType t, int b) {
    const bool ps = t == PadType::PS4 || t == PadType::PS5, xb = t == PadType::Xbox;
    if (t == PadType::Other || t == PadType::None) return display_pad(b);
    switch (b) {
    case 4: return xb ? "VIEW" : t == PadType::PS4 ? "SHARE" : "CREATE";
    case 5: return xb ? "XBOX" : "PS";
    case 6: return xb ? "MENU" : "OPTIONS";
    case 7: return "L3";
    case 8: return "R3";
    case 9: return xb ? "LB" : "L1";
    case 10: return xb ? "RB" : "R1";
    case 15: return xb ? "SHARE" : t == PadType::PS5 ? "MIC" : "MISC";
    case 16:
    case 17:
    case 18:
    case 19: return "P" + std::to_string(b - 15);
    case 20: return "TOUCH";
    case controls::kPadLeftTrigger: return xb ? "LT" : "L2";
    case controls::kPadRightTrigger: return xb ? "RT" : "R2";
    case 0:
    case 1:
    case 2:
    case 3: return ps ? "" : std::string(1, "ABXY"[b]);
    default: return display_pad(b);
    }
}
int glyph_width(PadType t, int b) {
    if ((t == PadType::PS4 || t == PadType::PS5) && b >= 0 && b <= 3) return 11;
    if (t == PadType::Xbox && b >= 0 && b <= 3) return 11;
    if (b < 0) return 8;
    return (int)glyph_label(t, b).size() * 8 + 4;
}
int draw_glyph(const Gfx &g, uint32_t *out, int x, int y, PadType t, int b, int maxw) {
    using namespace overlay;
    if (b < 0) {
        text(g, out, x, y, "-", 0x808080);
        return 8;
    }
    int w = glyph_width(t, b);
    if (w > maxw || t == PadType::Other || t == PadType::None) {
        std::string n = glyph_label(t, b);
        text(g, out, x, y, clip(n, (size_t)std::max(1, maxw / 8)).c_str(), 0xFFFFFF);
        return std::min<int>(maxw, (int)n.size() * 8);
    }
    const bool ps = t == PadType::PS4 || t == PadType::PS5;
    if (b >= 0 && b <= 3) {
        if (ps) {
            static const char *const *sh[4] = {kCross, kCircle, kSquare, kTriangle};
            static const uint32_t col[4] = {0x6C9EFF, 0xFF5050, 0xFF8CD2, 0x40E0A0};
            rect(out, x, y - 1, 11, 11, 0x202030);
            shape(out, x + 1, y, sh[b], col[b]);
        } else {
            static const uint32_t col[4] = {0x30C040, 0xE03030, 0x3070F0, 0xF0C020};
            disc(out, x, y - 1, 11, col[b]);
            text(g, out, x + 1, y, std::string(1, "ABXY"[b]).c_str(), 0x000000);
        }
        return 11;
    }
    std::string n = glyph_label(t, b);
    const bool trig = b == controls::kPadLeftTrigger || b == controls::kPadRightTrigger;
    rect(out, x, y - 1, w, 10, trig ? 0x504060 : 0x404050);
    frame_box(out, x, y - 1, w, 10, 0x9090B0);
    text(g, out, x + 2, y, n.c_str(), 0xFFFFFF);
    return w;
}
void draw_pad_icon(uint32_t *out, int x, int y, PadType t) {
    if (t == PadType::None) return;
    const uint32_t body = t == PadType::Xbox ? 0x40B050 : t == PadType::PS4 ? 0x4070E0 : t == PadType::PS5 ? 0xE0E0F0 : 0xA0A0A0;
    static const char *const shapeRows[10] = {"..XXXXXXXXXXXX..", ".XXXXXXXXXXXXXX.", "XXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXX",
                                              "XXXXXXXXXXXXXXXX", "XXXXXX....XXXXXX", "XXXXX......XXXXX", "XXXX........XXXX", ".XX..........XX."};
    for (int j = 0; j < 10; ++j)
        for (int i = 0; i < 16; ++i)
            if (shapeRows[j][i] == 'X') px(out, x + i, y + j, body);
    const uint32_t dark = 0x101018;
    px(out, x + 3, y + 3, dark);
    px(out, x + 2, y + 4, dark);
    px(out, x + 4, y + 4, dark);
    px(out, x + 3, y + 5, dark); // d-pad
    px(out, x + 12, y + 3, dark);
    px(out, x + 13, y + 4, dark);
    px(out, x + 11, y + 4, dark);
    px(out, x + 12, y + 5, dark); // face buttons
    px(out, x + 6, y + 6, dark);
    px(out, x + 9, y + 6, dark); // sticks
}
void draw_toast(const Gfx &g, uint32_t *out, PadType t, bool connected) {
    using namespace overlay;
    std::string msg = std::string(pad_type_word(t)) + (connected ? " CONNECTED" : " REMOVED");
    int w = 8 * (int)msg.size() + 28, x = (kPictureW - w) / 2;
    rect(out, x, 4, w, 18, 0x000028);
    frame_box(out, x, 4, w, 18, connected ? 0x00FF00 : 0xFF8040);
    draw_pad_icon(out, x + 5, 8, t);
    text(g, out, x + 24, 9, msg.c_str(), 0xFFFFFF);
}
void binding_lines(const controls::Bindings &b, std::string out[7]) {
    static const char *lab[7] = {"UP", "DOWN", "LEFT", "RIGHT", "COIN", "START1", "START2"};
    for (int i = 0; i < 7; ++i) {
        std::string l = lab[i];
        while (l.size() < 7) l += ' ';
        std::string k1 = display_key(b.key[i][0]), k2 = display_key(b.key[i][1]);
        std::string s = l + clip(k1, 9);
        if (b.key[i][1] && s.size() + 1 + k2.size() <= 16) s += "," + k2;
        out[i] = s;
    }
}
void binding_keycaps(const controls::Bindings &b, std::string out[7][2]) {
    for (int i = 0; i < 7; ++i)
        for (int k = 0; k < 2; ++k) {
            std::string n = display_key(b.key[i][k]);
            out[i][k] = n == "-" ? "" : clip(n, 7);
        }
}
void pad_names(const controls::Bindings &b, std::string out[5]) {
    for (int i = 0; i < 5; ++i) {
        int v = b.pad[controls::PadMenu + i];
        out[i] = v < 0 ? "" : display_pad(v);
    }
}
void pad_buttons(const controls::Bindings &b, int out[5]) {
    for (int i = 0; i < 5; ++i) out[i] = b.pad[controls::PadMenu + i];
}
// The Bindings screen's box (keep in step with draw_bindings below).
static const int kBindBx = 8, kBindBy = 6, kBindRowH = 15, kBindRow0 = 40;
bool bindings_cell_at(int px, int py, int *row, int *col) {
    const int bw = overlay::kOutW - 16;
    const int c1 = kBindBx + 8 + 8 * 9 - 3, c2 = kBindBx + 8 + 8 * 20 + 4 - 3, c3 = kBindBx + 8 + 8 * 30 - 3; // left edge of the KEY 1, KEY 2 and PAD BUTTON slots
    const int y0 = kBindBy + kBindRow0 - 3;                                                                   // the highlight of row r starts 3 pixels above its text
    if (px < kBindBx + 4 || px >= kBindBx + bw - 4) return false;
    int rel = py - y0;
    if (rel < 0 || rel >= controls::BindEditor::kRows * kBindRowH) return false;
    int r = rel / kBindRowH, c;
    if (px < c1) c = r >= controls::kActions ? 2 : 0; // the command's name
    else c = px < c2 ? 0 : px < c3 ? 1 : 2;
    *row = r;
    *col = c;
    return true;
}
bool help_pin_at(int px, int py) { return px >= overlay::kPictureW && px < overlay::kOutW && py >= 4 && py < 20; }

void draw_bindings(const Gfx &g, uint32_t *out, const controls::BindEditor &e, bool pad_connected, PadType type) {
    using namespace overlay;
    for (int y = 0; y < kOutH; ++y)
        for (int x = 0; x < kOutW; ++x) {
            uint32_t &p = out[y * kOutW + x];
            p = (p >> 2) & 0x3F3F3F;
        }
    const int bx = kBindBx, by = kBindBy, bw = kOutW - 16, rowh = kBindRowH, bh = 44 + controls::BindEditor::kRows * rowh + 40;
    rect(out, bx, by, bw, bh, 0x000028);
    frame_box(out, bx, by, bw, bh, 0xFFFF00);
    text(g, out, bx + 8, by + 8, "BINDINGS", 0xFFFFFF);
    {
        std::string hd = pad_connected ? std::string(pad_type_word(type)) + " PAD" : std::string("NO PAD CONNECTED");
        int hw = 8 * (int)hd.size();
        text(g, out, bx + bw - 8 - hw, by + 8, hd.c_str(), pad_connected ? 0x00FF00 : 0x808080);
        if (pad_connected) draw_pad_icon(out, bx + bw - 8 - hw - 20, by + 7, type);
    }
    const int cx[4] = {bx + 8, bx + 8 + 8 * 9, bx + 8 + 8 * 20 + 4, bx + 8 + 8 * 30}; // columns 10 characters wide
    const char *hd[4] = {"COMMAND", "KEY 1", "KEY 2", "PAD BUTTON"};
    for (int c = 0; c < 4; ++c) text(g, out, cx[c], by + 24, hd[c], 0x00FFFF);
    static const char *lab[controls::BindEditor::kRows] = {"UP", "DOWN", "LEFT", "RIGHT", "COIN", "START 1", "START 2", "MENU", "PAUSE", "SAVE", "LOAD", "REWIND"};
    const controls::Bindings &b = e.bindings();
    for (int r = 0; r < controls::BindEditor::kRows; ++r) {
        int y = by + kBindRow0 + r * rowh;
        text(g, out, cx[0], y, lab[r], 0xFFFFFF);
        const bool sysrow = r >= controls::kActions;
        std::string v[3] = {sysrow ? std::string(controls::fixed_key_label(r)) : display_key(b.key[r][0]), sysrow ? std::string("-") : display_key(b.key[r][1]),
                            r < controls::Coin ? std::string("DPAD+STICK") : display_pad(b.pad[r - controls::Coin])};
        for (int c = 0; c < 3; ++c) {
            bool sel = e.row() == r && e.col() == c;
            bool fixed = (c == 2 && r < controls::Coin) || (sysrow && c < 2);
            int w = 8 * 10;
            if (sel) rect(out, cx[c + 1] - 3, y - 3, w + 6, 13, e.waiting() ? 0x902020 : 0x303090);
            std::string t = (sel && e.waiting()) ? std::string("PRESS...") : clip(v[c], 10);
            const int pb = (c == 2 && r >= controls::Coin) ? b.pad[r - controls::Coin] : -1;
            if (c == 2 && pb >= 0 && !(sel && e.waiting())) draw_glyph(g, out, cx[c + 1], y, type, pb, w); // S34C: the button's own picture
            else text(g, out, cx[c + 1], y, t.c_str(), fixed ? 0x707070 : sel ? 0xFFFF00 : 0xFFFFFF);
        }
    }
    int fy = by + 40 + controls::BindEditor::kRows * rowh + 4;
    if (!e.message().empty()) text(g, out, bx + 8, fy, clip(e.message(), 40).c_str(), 0x00FFFF);
    text(g, out, bx + 8, fy + 12, clip(e.prompt(), 40).c_str(), 0xFFFFFF);
}

// ---- settings file: "name value" lines
bool load_settings(const std::string &path, Settings &s, std::string *problems) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    auto b = [](const std::string &v) { return v == "1" || v == "on" || v == "yes" || v == "true"; };
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string k, v;
        ss >> k >> v;
        if (k == "purist") s.purist = b(v);
        else if (k == "overlay") s.overlay = b(v);
        else if (k == "scanlines") s.scanlines = b(v);
        else if (k == "fill") s.fill = b(v);
        else if (k == "smooth") s.smooth = b(v);
        else if (k == "window_frame") s.frame = b(v);
        else if (k == "help_panel") s.help_pin = b(v);
        else if (k == "lives") {
            int x = std::atoi(v.c_str());
            s.lives = x == 1 ? 0 : x == 2 ? 1 : x == 5 ? 3 : 2;
        } else if (k == "bonus") {
            int x = std::atoi(v.c_str());
            s.bonus = x == 15000 ? 1 : x == 20000 ? 2 : v == "none" ? 3 : 0;
        } else if (k == "pad_symbols") s.pad_symbols = v == "xbox" ? 1 : v == "ps4" ? 2 : v == "ps5" ? 3 : 0;
        else if (k == "pause_on_focus_loss") s.pause_focus = b(v);
        else if (k == "lightbar") s.lightbar = b(v);
        else if (k == "run_summary") s.run_summary = b(v);
        else if (k == "surround") s.surround = v == "cabinet" ? 1 : v == "ambient" ? 2 : 0;
        else if (k == "curve") s.curve = v == "light" ? 1 : v == "strong" ? 2 : 0;
        else if (k == "glow") s.glow = v == "light" ? 1 : v == "strong" ? 2 : 0;
        else if (k == "crt_mask") s.crt_mask = v == "light" ? 1 : v == "strong" ? 2 : 0;
        else if (k == "hints") s.hints = v == "keyboard" ? 1 : v == "pad" ? 2 : 0;
        else if (k == "vibration") s.vibration = v == "off" ? 0 : v == "light" ? 1 : 2;
        else if (k == "difficulty") s.diff_normal = v != "hard";
        else if (k == "ghost_names") s.names_normal = v != "alternate";
        else if (k == "rules") s.rules_readable = v == "readable";
        else if (problems) *problems += "  ignored: " + line + "\n";
    }
    return true;
}
bool save_settings(const std::string &path, const Settings &s, std::string *error) {
    if (path.empty()) {
        if (error) *error = "no settings folder";
        return false;
    }
    static const int lv[] = {1, 2, 3, 5};
    static const char *bn[] = {"10000", "15000", "20000", "none"};
    std::string tmp = path + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) {
        if (error) *error = "cannot write " + tmp;
        return false;
    }
    std::fprintf(f,
                 "# Pac-Man port settings (the menu writes this file; you may also edit it with Notepad)\npurist %s\noverlay %s\nscanlines %s\nfill %s\nsmooth "
                 "%s\nwindow_frame %s\nhelp_panel %s\n"
                 "lives %d\nbonus %s\ndifficulty %s\nghost_names %s\nvibration %s\npad_symbols %s\npause_on_focus_loss %s\nhints %s\nlightbar %s\nrun_summary %s\n",
                 s.purist ? "on" : "off", s.overlay ? "on" : "off", s.scanlines ? "on" : "off", s.fill ? "on" : "off", s.smooth ? "on" : "off", s.frame ? "on" : "off",
                 s.help_pin ? "on" : "off", lv[s.lives & 3], bn[s.bonus & 3], s.diff_normal ? "normal" : "hard", s.names_normal ? "normal" : "alternate",
                 s.vibration == 0   ? "off"
                 : s.vibration == 1 ? "light"
                                    : "full",
                 s.pad_symbols == 1   ? "xbox"
                 : s.pad_symbols == 2 ? "ps4"
                 : s.pad_symbols == 3 ? "ps5"
                                      : "auto",
                 s.pause_focus ? "on" : "off",
                 s.hints == 1   ? "keyboard"
                 : s.hints == 2 ? "pad"
                                : "auto",
                 s.lightbar ? "on" : "off", s.run_summary ? "on" : "off");
    std::fprintf(f, "surround %s\n", s.surround == 1 ? "cabinet" : s.surround == 2 ? "ambient" : "black"); // X44, X46
    std::fprintf(f, "curve %s\n", s.curve == 1 ? "light" : s.curve == 2 ? "strong" : "off");               // X42
    std::fprintf(f, "glow %s\n", s.glow == 1 ? "light" : s.glow == 2 ? "strong" : "off");                  // X41
    std::fprintf(f, "crt_mask %s\n", s.crt_mask == 1 ? "light" : s.crt_mask == 2 ? "strong" : "off");      // X40
    std::fprintf(f, "rules %s\n", s.rules_readable ? "readable" : "original");                              // T02i
    std::fclose(f);
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        if (error) *error = "cannot rename " + tmp;
        return false;
    }
    return true;
}
} // namespace menu
