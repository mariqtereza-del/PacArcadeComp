// X48: integration checks that need no window. (1) the settings file: every setting set away from its default is written once, in a tidy "key value" form, read back, and written again byte for byte
// (2) the Purist table: which menu commands Purist locks, compared with the agreed list (every extra locked, nothing else). Prints the settings table for docs/notes/settings_table.md.
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <SDL.h>
#include "../src/board/romset.h"
#include "../src/video/gfx.h"
#include "../src/app/controls.cpp"
#include "../src/app/overlay.cpp"
#include "../src/app/menu.cpp"
static int n = 0, fails = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
static std::string slurp(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}
int main(int argc, char **argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    const std::string a = dir + "/int_a.cfg", b = dir + "/int_b.cfg";
    menu::Settings s;
    s.purist = true;
    s.overlay = true;
    s.scanlines = true;
    s.fill = true;
    s.smooth = true;
    s.frame = false;
    s.help_pin = true;
    s.lives = 3;
    s.bonus = 2;
    s.diff_normal = false;
    s.names_normal = false;
    s.vibration = 1;
    s.pause_focus = false;
    s.hints = 2;
    s.lightbar = false;
    s.run_summary = false;
    s.crt_mask = 2;
    s.glow = 1;
    s.curve = 2;
    s.surround = 2;
    s.pad_symbols = 3;
    s.rules_readable = true; // T02i
    std::string err;
    CHECK(menu::save_settings(a, s, &err), "settings saved");
    const std::string text = slurp(a);
    std::istringstream in(text);
    std::string line;
    std::map<std::string, std::string> kv;
    std::set<std::string> seen;
    bool tidy = true, dup = false;
    int comments = 0;
    while (std::getline(in, line)) {
        if (line.empty()) {
            tidy = false;
            continue;
        }
        if (line[0] == '#') {
            ++comments;
            continue;
        }
        const size_t sp = line.find(' ');
        if (sp == std::string::npos || sp == 0 || line.find(' ', sp + 1) != std::string::npos || line.back() == '\r') {
            tidy = false;
            continue;
        }
        const std::string k = line.substr(0, sp);
        if (!seen.insert(k).second) dup = true;
        kv[k] = line.substr(sp + 1);
    }
    CHECK(tidy && !dup && comments == 1, "settings file: one comment line, then one 'key value' line per setting, no blank lines, no repeated key");
    CHECK(kv.size() == 22, "settings file: 22 settings (every field of the Settings table)");
    menu::Settings t;
    std::string prob;
    CHECK(menu::load_settings(a, t, &prob) && prob.empty(), "settings read back with no problems");
    CHECK(menu::save_settings(b, t, &err) && slurp(a) == slurp(b), "read back and written again: identical bytes");
    CHECK(t.purist && t.overlay && t.scanlines && t.fill && t.smooth && !t.frame && t.help_pin && t.lives == 3 && t.bonus == 2 && !t.diff_normal && !t.names_normal &&
              t.vibration == 1 && !t.pause_focus && t.hints == 2 && !t.lightbar && !t.run_summary && t.crt_mask == 2 && t.glow == 1 && t.curve == 2 && t.surround == 2 &&
              t.pad_symbols == 3 && t.rules_readable,
          "every value survived the round trip");
    {
        std::ofstream f(dir + "/int_bad.cfg");
        f << "glow banana\nunknown_key 5\nsurround ambient\n";
    }
    menu::Settings u;
    std::string p2;
    CHECK(menu::load_settings(dir + "/int_bad.cfg", u, &p2) && !p2.empty() && u.surround == 2 && u.glow == 0,
          "a bad value and an unknown key are reported, the good line is still used");
    std::printf("SETTINGS TABLE (key: value written for the non-default test settings)\n");
    for (const auto &e : kv) std::printf("  %-22s %s\n", e.first.c_str(), e.second.c_str());
    // the Purist table
    menu::Model m;
    m.purist = true;
    menu::Model off;
    off.purist = false;
    const struct {
        menu::Cmd c;
        const char *name;
        bool locked;
    } tab[] = {{menu::Cmd::Resume, "Resume", false},
               {menu::Cmd::Bindings, "Bindings", false},
               {menu::Cmd::SaveState, "SaveState", true},
               {menu::Cmd::LoadState, "LoadState", true},
               {menu::Cmd::Lives, "Lives", false},
               {menu::Cmd::Bonus, "Bonus", false},
               {menu::Cmd::Difficulty, "Difficulty", false},
               {menu::Cmd::Names, "Names", false},
               {menu::Cmd::Restart, "Restart", false},
               {menu::Cmd::Keys, "Keys", false},
               {menu::Cmd::Pad, "Pad", false},
               {menu::Cmd::ResetControls, "ResetControls", false},
               {menu::Cmd::OneButton, "OneButton", true},
               {menu::Cmd::Overlay, "Overlay", true},
               {menu::Cmd::Scanlines, "Scanlines", true},
               {menu::Cmd::Pacing, "Pacing", true},
               {menu::Cmd::Frame, "Frame", false},
               {menu::Cmd::Scaling, "Scaling", true},
               {menu::Cmd::FullScreen, "FullScreen", false},
               {menu::Cmd::HelpPin, "HelpPin", false},
               {menu::Cmd::Purist, "Purist", false},
               {menu::Cmd::Quit, "Quit", false},
               {menu::Cmd::Vibration, "Vibration", true},
               {menu::Cmd::PadSymbols, "PadSymbols", false},
               {menu::Cmd::FocusPause, "FocusPause", true},
               {menu::Cmd::Hints, "Hints", true},
               {menu::Cmd::Lightbar, "Lightbar", true},
               {menu::Cmd::RunSummary, "RunSummary", true},
               {menu::Cmd::CrtMask, "CrtMask", true},
               {menu::Cmd::Glow, "Glow", true},
               {menu::Cmd::Curve, "Curve", true},
               {menu::Cmd::CrtLook, "CrtLook", true},
               {menu::Cmd::Surround, "Surround", true},
               {menu::Cmd::SaveSlot, "SaveSlot", true},
               {menu::Cmd::LoadSlot, "LoadSlot", true},
               {menu::Cmd::Rules, "Rules", true},
               {menu::Cmd::WriteRules, "WriteRules", false}};
    std::printf("PURIST TABLE (menu command: locked in Purist?)\n");
    int locked_n = 0;
    for (const auto &e : tab) {
        const bool l = menu::locked(e.c, m);
        locked_n += l;
        std::printf("  %-14s %s\n", e.name, l ? "locked" : "available");
        CHECK(l == e.locked && !menu::locked(e.c, off), (std::string("Purist table: ") + e.name).c_str());
    }
    CHECK(locked_n == 20, "20 commands are locked in Purist");
    CHECK(menu::value_text(menu::Cmd::Surround, m) == "BLACK" && menu::value_text(menu::Cmd::Glow, m) == "OFF" && menu::value_text(menu::Cmd::Curve, m) == "OFF" &&
              menu::value_text(menu::Cmd::CrtMask, m) == "OFF",
          "Purist shows the visual extras as off");
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
