// S34 test of src/app/menu.{h,cpp}: navigation, Purist locks, every function key is in the menu, settings file round trip, drawing stays inside its buffer.
// Usage: test_menu pacman.zip (build/run: tests/test_menu.sh)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/romset.h"
#include "../src/video/gfx.h"
#include <SDL.h>
#include "../src/app/controls.cpp"
#include "../src/app/overlay.cpp"
#include "../src/app/menu.cpp"
using namespace menu;
static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_menu pacman.zip\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    static Gfx g;
    if (!load_romset_from_file(argv[1], roms, err) || !decode_gfx(roms, g)) {
        std::printf("FAIL: ROM: %s\n", err.c_str());
        return 1;
    }
    Model m;
    Menu mn;
    mn.open();
    // navigation: main page items in order, Down wraps, Enter on GAME OPTIONS opens page 1, Back returns, Back on the main page resumes
    expect(mn.page() == 0 && mn.cursor() == 0, "opens on the first item of the main page");
    expect(mn.key(Key::Enter, m).cmd == Cmd::Resume, "Enter on RESUME resumes");
    expect(mn.key(Key::Up, m).cmd == Cmd::None && mn.cursor() == 10, "Up from the first item wraps to the last (QUIT GAME)");
    expect(mn.key(Key::Enter, m).cmd == Cmd::Quit, "Enter on QUIT GAME quits");
    mn.key(Key::Down, m);
    for (int i = 0; i < 3; ++i) mn.key(Key::Down, m);
    expect(mn.cursor() == 3, "cursor on GAME OPTIONS");
    expect(mn.key(Key::Enter, m).cmd == Cmd::None && mn.page() == 1, "Enter on GAME OPTIONS opens page 1");
    Result r = mn.key(Key::Right, m);
    expect(r.cmd == Cmd::Lives && r.delta == 1, "Right on LIVES = +1");
    r = mn.key(Key::Left, m);
    expect(r.cmd == Cmd::Lives && r.delta == -1, "Left on LIVES = -1");
    mn.key(Key::Down, m);
    expect(mn.key(Key::Right, m).cmd == Cmd::Bonus, "second item is BONUS LIFE");
    mn.key(Key::Down, m);
    expect(mn.key(Key::Right, m).cmd == Cmd::Difficulty, "third item is DIFFICULTY");
    mn.key(Key::Down, m);
    expect(mn.key(Key::Right, m).cmd == Cmd::Names, "fourth item is GHOST NAMES");
    mn.key(Key::Down, m);
    expect(mn.key(Key::Enter, m).cmd == Cmd::Restart, "fifth item restarts the game");
    expect(mn.key(Key::Back, m).cmd == Cmd::None && mn.page() == 0, "Back returns to the main page");
    expect(mn.key(Key::Back, m).cmd == Cmd::Resume, "Back on the main page resumes");
    {   // the cheats have their own page (main page item CHEATS), and are not in GAME OPTIONS any more
        Menu c; c.open();
        int at = -1; for (int i = 0; i < 11; ++i) { if (c.visible_lines(m)[c.cursor()] == "CHEATS") { at = i; break; } c.key(Key::Down, m); }
        expect(at >= 0, "the main page has a CHEATS item");
        expect(c.key(Key::Enter, m).cmd == Cmd::None && c.page() == 8, "Enter on CHEATS opens page 8");
        expect(c.visible_lines(m)[0] == "INVINCIBLE        OFF" || c.visible_lines(m)[0].rfind("INVINCIBLE", 0) == 0, "the CHEATS page starts with INVINCIBLE");
        expect(c.key(Key::Enter, m).cmd == Cmd::CheatInvincible, "Enter on INVINCIBLE gives the cheat command");
        c.key(Key::Down, m);
        expect(c.key(Key::Enter, m).cmd == Cmd::None && c.page() == 0, "BACK on the CHEATS page returns to the main page");
        Menu g2; g2.open(); for (int i = 0; i < 3; ++i) g2.key(Key::Down, m); g2.key(Key::Enter, m);
        bool inv = false; for (const std::string &l : g2.visible_lines(m)) if (l.rfind("INVINCIBLE", 0) == 0) inv = true;
        expect(!inv, "GAME OPTIONS has no INVINCIBLE item");
        Model pm = m; pm.purist = true; Menu p2; p2.open(); for (int i = 0; i < at; ++i) p2.key(Key::Down, pm); p2.key(Key::Enter, pm);
        expect(p2.key(Key::Enter, pm).cmd == Cmd::Locked, "INVINCIBLE is locked in Purist mode");
    }
    // every function key has a menu item whose label names it
    struct FK {
        Cmd c;
        const char *k;
    } fks[] = {{Cmd::SaveState, "F2"}, {Cmd::LoadState, "F4"}, {Cmd::Overlay, "F3"}, {Cmd::Scanlines, "F5"},   {Cmd::Pacing, "F6"},        {Cmd::Frame, "F7"},
               {Cmd::Scaling, "F8"},   {Cmd::Keys, "F9"},      {Cmd::Pad, "F10"},    {Cmd::FullScreen, "F11"}, {Cmd::ResetControls, "F12"}};
    for (const FK &f : fks) expect(label_of(f.c).find(f.k) != std::string::npos, std::string("menu item for ") + f.k + " is labelled with it (" + label_of(f.c) + ")");
    // values
    m.lives = 3;
    m.bonus = 3;
    m.diff_normal = false;
    m.names_normal = false;
    expect(value_text(Cmd::Lives, m) == "5" && value_text(Cmd::Bonus, m) == "NONE" && value_text(Cmd::Difficulty, m) == "HARD" &&
               value_text(Cmd::Names, m) == "ALTERNATE",
           "DIP values are shown in words");
    m = Model();
    {   // S34E: no Purist item anywhere; the two RESET TO DEFAULTS items sit on their own pages
        bool any = false;
        for (int c = 0; c <= (int)Cmd::Locked; ++c) if (label_of((Cmd)c).find("PURIST") != std::string::npos) any = true;
        expect(!any, "S34E: no menu item is called PURIST");
        expect(label_of(Cmd::ResetDisplay) == "RESET TO DEFAULTS" && label_of(Cmd::ResetGame) == "RESET TO DEFAULTS", "S34E: both reset items are labelled RESET TO DEFAULTS");
        Menu a; a.open(); m.purist = false;
        while (a.page() != 3) { a.key(Key::Down, m); if (a.cursor() == 5 && a.page() == 0) { a.key(Key::Enter, m); } if (a.page() == 0 && a.cursor() > 9) break; }
        expect(a.page() == 3, "S34E: the DISPLAY page opens");
        bool found = false;
        for (int i = 0; i < 20 && !found; ++i) { if (a.visible_lines(m)[a.cursor()] == "RESET TO DEFAULTS") found = true; else a.key(Key::Down, m); }
        expect(found && a.key(Key::Enter, m).cmd == Cmd::ResetDisplay, "S34E: DISPLAY has RESET TO DEFAULTS and Enter on it gives the display reset");
        Menu g; g.open();
        for (int i = 0; i < 3; ++i) g.key(Key::Down, m);
        g.key(Key::Enter, m);
        bool fg = false;
        for (int i = 0; i < 20 && !fg; ++i) { if (g.visible_lines(m)[g.cursor()] == "RESET TO DEFAULTS") fg = true; else g.key(Key::Down, m); }
        expect(g.page() == 1 && fg && g.key(Key::Enter, m).cmd == Cmd::ResetGame, "S34E: GAME OPTIONS has RESET TO DEFAULTS and Enter on it gives the game reset");
    }
    // Purist: the additions are locked, the arcade's own options and the pause are not
    m.purist = true;
    Cmd locks[] = {Cmd::SaveState, Cmd::LoadState, Cmd::OneButton, Cmd::Vibration, Cmd::Overlay, Cmd::Scanlines, Cmd::Pacing, Cmd::Scaling};
    for (Cmd c : locks) expect(locked(c, m), "locked in Purist: " + label_of(c));
    Cmd frees[] = {Cmd::Resume, Cmd::Lives, Cmd::Bonus, Cmd::Difficulty, Cmd::Names, Cmd::Restart, Cmd::Keys, Cmd::Frame, Cmd::FullScreen, Cmd::ResetDisplay, Cmd::ResetGame, Cmd::Quit};
    for (Cmd c : frees) expect(!locked(c, m), "still free in Purist: " + label_of(c));
    m.overlay = true;
    m.scanlines = true;
    m.smooth = true;
    m.fill = true;
    m.one_button = true;
    expect(value_text(Cmd::Overlay, m) == "OFF" && value_text(Cmd::Scanlines, m) == "OFF" && value_text(Cmd::Pacing, m) == "EXACT" &&
               value_text(Cmd::Scaling, m) == "WHOLE" && value_text(Cmd::OneButton, m) == "OFF",
           "Purist shows the additions as off whatever was chosen");
    m.vibration = 2;
    expect(value_text(Cmd::Vibration, m) == "OFF", "S34R: Purist shows vibration as off");
    Menu mp;
    mp.open();
    mp.key(Key::Down, m); // SAVE STATE
    expect(mp.key(Key::Enter, m).cmd == Cmd::Locked, "Enter on a locked item gives Locked, not the command");
    m.purist = false;
    Menu mq;
    mq.open();
    mq.key(Key::Down, m);
    expect(mq.key(Key::Enter, m).cmd == Cmd::None && mq.page() == 4, "X12: outside Purist the same item opens the save picker (page 4)");
    {
        Model sm;
        sm.slot_line[2] = "3 10-14 0420 L2 12340";
        auto lines = mq.visible_lines(sm);
        expect(lines.size() == 9 && lines[0] == "1 EMPTY" && lines[2] == "3 10-14 0420 L2 12340" && lines[8] == "BACK",
               "X12: eight slot lines (EMPTY when not filled in) and BACK");
        mq.key(Key::Down, sm);
        mq.key(Key::Down, sm);
        Result r = mq.key(Key::Enter, sm);
        expect(r.cmd == Cmd::SaveSlot && r.delta == 3, "X12: Enter on the third slot saves slot 3");
        expect(mq.key(Key::Left, sm).cmd == Cmd::None, "X12: Left does nothing on a slot");
        mq.key(Key::Back, sm);
        expect(mq.page() == 0 && mq.cursor() == 1, "X12: Esc goes back to SAVE STATE on the main page");
        mq.key(Key::Down, sm);
        mq.key(Key::Enter, sm);
        expect(mq.page() == 5, "X12: LOAD STATE opens the load picker (page 5)");
        for (int i = 0; i < 7; ++i) mq.key(Key::Down, sm);
        r = mq.key(Key::Enter, sm);
        expect(r.cmd == Cmd::LoadSlot && r.delta == 8, "X12: Enter on the eighth slot loads slot 8");
        mq.key(Key::Down, sm);
        expect(mq.visible_lines(sm)[mq.cursor()] == "BACK" && mq.key(Key::Enter, sm).cmd == Cmd::None && mq.page() == 0, "X12: BACK leaves the picker");
        Model pm2;
        pm2.purist = true;
        Menu mp2;
        mp2.open();
        mp2.key(Key::Down, pm2);
        mp2.key(Key::Down, pm2);
        expect(mp2.key(Key::Enter, pm2).cmd == Cmd::Locked && mp2.page() == 0, "X12: LOAD STATE is locked in Purist (the picker does not open)");
        Menu mm2;
        mm2.open();
        mm2.key(Key::Down, sm);
        mm2.key(Key::Enter, sm);
        int px = 100, py = 46 + 14 * 4 + 6;
        Result rc = mm2.click(px, py, sm);
        expect(rc.cmd == Cmd::SaveSlot && rc.delta == 5, "X12: a mouse click on a slot row saves that slot");
        expect(label_of(Cmd::SaveState) == "SAVE STATE F2" && label_of(Cmd::LoadState) == "LOAD STATE F4", "X12: the menu labels are still found");
    }
    {
        Model v;
        v.vibration = 0;
        expect(value_text(Cmd::Vibration, v) == "OFF", "S34R: vibration off");
        v.vibration = 1;
        expect(value_text(Cmd::Vibration, v) == "LIGHT", "S34R: vibration light");
        v.vibration = 2;
        expect(value_text(Cmd::Vibration, v) == "FULL", "S34R: vibration full");
        Menu mv;
        mv.open();
        for (int i = 0; i < 4; ++i) mv.key(Key::Down, v);
        mv.key(Key::Enter, v);                            // CONTROLS
        for (int i = 0; i < 4; ++i) mv.key(Key::Down, v); // BINDINGS, CHANGE KEYS, CHANGE PAD, RESET KEYS -> ONE BUTTON START
        mv.key(Key::Down, v);
        Result r = mv.key(Key::Right, v);
        expect(r.cmd == Cmd::Vibration && r.delta == 1, "S34R: Right on VIBRATION cycles it forward");
        r = mv.key(Key::Left, v);
        expect(r.cmd == Cmd::Vibration && r.delta == -1, "S34R: Left cycles it back");
    }
    // S34C: pad types and picture labels
    expect(pad_type_of_sdl(1, true) == PadType::Xbox && pad_type_of_sdl(2, true) == PadType::Xbox && pad_type_of_sdl(4, true) == PadType::PS4 &&
               pad_type_of_sdl(3, true) == PadType::PS4 && pad_type_of_sdl(7, true) == PadType::PS5 && pad_type_of_sdl(0, true) == PadType::Other &&
               pad_type_of_sdl(4, false) == PadType::Other,
           "S34C: SDL controller types map to Xbox, PS4, PS5, other");
    expect(std::string(pad_type_word(PadType::PS5)) == "PS5" && std::string(pad_type_word(PadType::Xbox)) == "XBOX" &&
               std::string(pad_type_word(PadType::Other)) == "PAD" && std::string(pad_type_word(PadType::None)).empty(),
           "S34C: icon words");
    expect(glyph_label(PadType::Xbox, 6) == "MENU" && glyph_label(PadType::PS4, 6) == "OPTIONS" && glyph_label(PadType::Xbox, 4) == "VIEW" &&
               glyph_label(PadType::PS4, 4) == "SHARE" && glyph_label(PadType::PS5, 4) == "CREATE",
           "S34C: Back and Start names per type");
    expect(glyph_label(PadType::PS5, 15) == "MIC" && glyph_label(PadType::Xbox, 15) == "SHARE" && glyph_label(PadType::PS5, 20) == "TOUCH",
           "S34C: Share/Mic and touchpad names");
    expect(glyph_label(PadType::Xbox, controls::kPadLeftTrigger) == "LT" && glyph_label(PadType::PS4, controls::kPadLeftTrigger) == "L2" &&
               glyph_label(PadType::PS5, controls::kPadRightTrigger) == "R2" && glyph_label(PadType::Xbox, 9) == "LB" && glyph_label(PadType::PS4, 10) == "R1",
           "S34C: shoulder and trigger names");
    expect(glyph_label(PadType::PS4, 0).empty() && glyph_label(PadType::Xbox, 0) == "A" && glyph_label(PadType::Xbox, 3) == "Y" && glyph_width(PadType::PS4, 2) == 11 &&
               glyph_width(PadType::Xbox, 1) == 11,
           "S34C: face buttons are pictures (PS) or lettered discs (Xbox)");
    expect(glyph_label(PadType::Other, 0) == display_pad(0) && glyph_label(PadType::Other, controls::kPadLeftTrigger) == "L2/LT",
           "S34C: unknown pads keep the plain names");
    {
        int worst = 0;
        const PadType ts[] = {PadType::Xbox, PadType::PS4, PadType::PS5};
        for (PadType t : ts)
            for (int b = 0; b <= 20; ++b) {
                if (b >= 11 && b <= 14) continue;
                worst = std::max(worst, glyph_width(t, b));
            }
        expect(worst <= 80, "S34C: every picture fits the 80-pixel pad column");
    }
    {
        Model v;
        expect(value_text(Cmd::PadSymbols, v) == "AUTO", "S34C: pad symbols default AUTO");
        v.pad_symbols = 1;
        expect(value_text(Cmd::PadSymbols, v) == "XBOX", "S34C: pad symbols XBOX");
        v.pad_symbols = 2;
        expect(value_text(Cmd::PadSymbols, v) == "PS4", "S34C: PS4");
        v.pad_symbols = 3;
        expect(value_text(Cmd::PadSymbols, v) == "PS5", "S34C: PS5");
        expect(!locked(Cmd::PadSymbols, v), "S34C: pad symbols are not locked in Purist");
        Menu mv;
        mv.open();
        for (int i = 0; i < 4; ++i) mv.key(Key::Down, v);
        mv.key(Key::Enter, v);
        for (int i = 0; i < 6; ++i) mv.key(Key::Down, v); // CONTROLS page, PAD SYMBOLS is the 7th item
        Result r = mv.key(Key::Right, v);
        expect(r.cmd == Cmd::PadSymbols && r.delta == 1, "S34C: Right on PAD SYMBOLS cycles it");
    }
    // X40: CRT MASK row
    {
        Model v;
        expect(value_text(Cmd::CrtMask, v) == "OFF", "X40: CRT mask default OFF");
        v.crt_mask = 1;
        expect(value_text(Cmd::CrtMask, v) == "LIGHT", "X40: LIGHT");
        v.crt_mask = 2;
        expect(value_text(Cmd::CrtMask, v) == "STRONG", "X40: STRONG");
        expect(!locked(Cmd::CrtMask, v), "X40: free outside Purist");
        v.purist = true;
        expect(locked(Cmd::CrtMask, v) && value_text(Cmd::CrtMask, v) == "OFF", "X40: locked in Purist and shown as OFF whatever was chosen");
        v.purist = false;
        Menu mc;
        mc.open();
        for (int i = 0; i < 3; ++i) mc.key(Key::Down, v);
        (void)mc;
        Settings cs;
        cs.crt_mask = 2;
        std::string cp = "build/s34/crt_settings_test.cfg", ce;
        expect(save_settings(cp, cs, &ce), "X40: settings saved");
        {   // N49: the settings folder does not exist yet (the first run of a new test folder): it is created
            std::string np = "build/s34/new_folder_n49/inner/settings.cfg", ne;
            std::remove(np.c_str());
            expect(save_settings(np, cs, &ne), "N49: settings saved into a folder that did not exist yet");
        }
        Settings ct;
        load_settings(cp, ct, nullptr);
        expect(ct.crt_mask == 2, "X40: crt_mask round trip (strong)");
        cs.crt_mask = 1;
        save_settings(cp, cs, &ce);
        load_settings(cp, ct, nullptr);
        expect(ct.crt_mask == 1, "X40: crt_mask round trip (light)");
    }
    // X41: GLOW row
    {
        Model v;
        expect(value_text(Cmd::Glow, v) == "OFF", "X41: glow default OFF");
        v.glow = 1;
        expect(value_text(Cmd::Glow, v) == "LIGHT", "X41: LIGHT");
        v.glow = 2;
        expect(value_text(Cmd::Glow, v) == "STRONG", "X41: STRONG");
        expect(!locked(Cmd::Glow, v), "X41: free outside Purist");
        v.purist = true;
        expect(locked(Cmd::Glow, v) && value_text(Cmd::Glow, v) == "OFF", "X41: locked in Purist and shown as OFF");
        Menu dm;
        dm.open();
        for (int i = 0; i < 5; ++i) dm.key(Key::Down, Model());
        dm.key(Key::Enter, Model());
        bool found = false;
        for (const std::string &l : dm.visible_lines(Model()))
            if (l.find("GLOW") == 0) {
                found = true;
                expect(l.size() <= 24, "GLOW line fits: " + l);
            }
        expect(found, "X41: the Display page has a GLOW item");
        Settings gs;
        gs.glow = 2;
        std::string gp = "build/s34/glow_settings_test.cfg", ge;
        expect(save_settings(gp, gs, &ge), "X41: settings saved");
        Settings gt;
        std::string gpr;
        load_settings(gp, gt, &gpr);
        expect(gt.glow == 2 && gpr.empty(), "X41: glow round trip (strong)");
        gs.glow = 1;
        save_settings(gp, gs, &ge);
        load_settings(gp, gt, nullptr);
        expect(gt.glow == 1, "X41: glow round trip (light)");
    }
    // X42: CURVE row
    {
        Model v;
        expect(value_text(Cmd::Curve, v) == "OFF", "X42: curve default OFF");
        v.curve = 1;
        expect(value_text(Cmd::Curve, v) == "LIGHT", "X42: LIGHT");
        v.curve = 2;
        expect(value_text(Cmd::Curve, v) == "STRONG", "X42: STRONG");
        expect(!locked(Cmd::Curve, v), "X42: free outside Purist");
        v.purist = true;
        expect(locked(Cmd::Curve, v) && value_text(Cmd::Curve, v) == "OFF", "X42: locked in Purist and shown as OFF");
        Menu dm;
        dm.open();
        for (int i = 0; i < 5; ++i) dm.key(Key::Down, Model());
        dm.key(Key::Enter, Model());
        bool found = false;
        for (const std::string &l : dm.visible_lines(Model()))
            if (l.find("CURVE") == 0) {
                found = true;
                expect(l.size() <= 24, "CURVE line fits: " + l);
            }
        expect(found, "X42: the Display page has a CURVE item");
        Settings cs;
        cs.curve = 2;
        std::string cp = "build/s34/curve_settings_test.cfg", ce;
        expect(save_settings(cp, cs, &ce), "X42: settings saved");
        Settings ct;
        std::string cpr;
        load_settings(cp, ct, &cpr);
        expect(ct.curve == 2 && cpr.empty(), "X42: curve round trip (strong)");
        cs.curve = 1;
        save_settings(cp, cs, &ce);
        load_settings(cp, ct, nullptr);
        expect(ct.curve == 1, "X42: curve round trip (light)");
    }
    // X43: CRT LOOK row on the Display page
    {
        Menu dm;
        dm.open();
        for (int i = 0; i < 5; ++i) dm.key(Key::Down, Model());
        dm.key(Key::Enter, Model());
        bool found = false;
        for (const std::string &l : dm.visible_lines(Model()))
            if (l.find("CRT LOOK") == 0) {
                found = true;
                expect(l.size() <= 24, "CRT LOOK line fits: " + l);
            }
        expect(found, "X43: the Display page has a CRT LOOK item");
        Model v;
        v.crt_mask = 2;
        v.glow = 2;
        v.curve = 2;
        for (const std::string &l : dm.visible_lines(v))
            if (l.find("CRT LOOK") == 0) expect(l.size() <= 24 && l.find("OLD TV") != std::string::npos, "X43: CRT LOOK line shows OLD TV: " + l);
        Model w;
        w.crt_mask = 1;
        w.glow = 1;
        w.scanlines = true;
        for (const std::string &l : dm.visible_lines(w))
            if (l.find("CRT LOOK") == 0) expect(l.size() <= 24 && l.find("ARCADE MONITOR") != std::string::npos, "X43: longest value fits: " + l);
    }
    // settings file
    Settings s;
    s.purist = true;
    s.overlay = true;
    s.lives = 3;
    s.bonus = 2;
    s.diff_normal = false;
    s.names_normal = false;
    s.help_pin = true;
    s.frame = false;
    s.fill = true;
    s.smooth = true;
    s.scanlines = true;
    s.vibration = 1;
    s.pad_symbols = 1;
    std::string path = "build/s34/settings_test.cfg";
    std::string e;
    expect(save_settings(path, s, &e), "settings saved " + e);
    Settings t;
    std::string pr;
    expect(load_settings(path, t, &pr) && pr.empty(), "settings loaded without problems");
    expect(!t.purist && t.overlay && t.lives == 3 && t.bonus == 2 && !t.diff_normal && !t.names_normal && t.help_pin && !t.frame && t.fill && t.smooth && t.scanlines &&
               t.vibration == 1 && t.pad_symbols == 1,
           "settings round trip");
    {   // S34D: Purist is not a saved setting any more: a line "purist on" from an old file is reported and ignored, and nothing is written for it
        FILE *f = std::fopen(path.c_str(), "ab");
        std::fputs("purist on\n", f);
        std::fclose(f);
        Settings o;
        std::string pr2;
        expect(load_settings(path, o, &pr2) && !o.purist && pr2.find("purist on") != std::string::npos && pr2.find("Reset to defaults") == std::string::npos && pr2.find("RESET TO DEFAULTS") != std::string::npos, "S34D: an old 'purist on' line is reported and ignored");
        std::string e2;
        expect(save_settings(path, o, &e2), "S34D: saved again");
        FILE *g2 = std::fopen(path.c_str(), "rb");
        std::string all;
        char buf[256];
        size_t n2;
        while ((n2 = std::fread(buf, 1, sizeof buf, g2)) > 0) all.append(buf, n2);
        std::fclose(g2);
        expect(all.find("purist") == std::string::npos, "S34D: the settings file has no purist line any more");
    }
    Settings d;
    expect(!load_settings("build/s34/does_not_exist.cfg", d, nullptr), "missing file reported as missing");
    {
        FILE *f = std::fopen(path.c_str(), "ab");
        std::fputs("nonsense 1\n", f);
        std::fclose(f);
        Settings u;
        std::string p2;
        load_settings(path, u, &p2);
        expect(!p2.empty(), "unknown lines are reported, not fatal");
    }
    // drawing: stays in the buffer, dims the picture, draws something in the panel; Purist changes the help panel
    static uint32_t a[overlay::kOutW * overlay::kOutH], b[overlay::kOutW * overlay::kOutH];
    for (auto &p : a) p = 0xFFFFFF;
    for (auto &p : b) p = 0xFFFFFF;
    Model dm;
    Menu dmn;
    dmn.open();
    dmn.draw(g, a, dm);
    draw_help(g, a, dm);
    int dimmed = 0, panel_lit = 0;
    for (int y = 0; y < overlay::kPictureH; ++y)
        for (int x = 0; x < overlay::kPictureW; ++x)
            if (a[y * overlay::kOutW + x] != 0xFFFFFF) ++dimmed;
    for (int y = 0; y < overlay::kOutH; ++y)
        for (int x = overlay::kPictureW; x < overlay::kOutW; ++x)
            if (a[y * overlay::kOutW + x] > 0x202020) ++panel_lit;
    expect(dimmed > 20000, "the picture is dimmed under the menu");
    expect(panel_lit > 500, "the help panel has text");
    dm.purist = true;
    dmn.draw(g, b, dm);
    draw_help(g, b, dm);
    expect(std::memcmp(a, b, sizeof a) != 0, "Purist changes what is drawn");
    // every page fits in the box
    for (int pg = 0; pg < 4; ++pg) {
        Menu x;
        x.open();
        if (pg) {
            for (int i = 0; i < 2 + pg; ++i) x.key(Key::Down, dm);
            x.key(Key::Enter, dm);
        }
        for (const std::string &l : x.visible_lines(dm)) expect(l.size() <= 24, "line fits the box: " + l);
    }
    // S34A: names, help lines from the real bindings, the bindings screen
    expect(display_key(SDL_SCANCODE_UP) == "UP" && display_key(SDL_SCANCODE_KP_5) == "KEYPAD 5" && display_key(SDL_SCANCODE_COMMA) == "COMMA" && display_key(0) == "-",
           "key names in the game font: " + display_key(SDL_SCANCODE_KP_5));
    expect(display_pad(SDL_CONTROLLER_BUTTON_BACK) == "BACK" && display_pad(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) == "LB" && display_pad(-1) == "-", "pad button names");
    {
        controls::Bindings bb;
        std::string ln[7];
        binding_lines(bb, ln);
        expect(ln[0] == "UP     UP,W" && ln[4] == "COIN   5" && ln[5] == "START1 1", "help lines show the bindings: [" + ln[0] + "] [" + ln[4] + "] [" + ln[5] + "]");
        bb.key[controls::Coin][0] = SDL_SCANCODE_C;
        bb.key[controls::Coin][1] = 0;
        binding_lines(bb, ln);
        expect(ln[4] == "COIN   C", "help line follows a changed binding: " + ln[4]);
        for (int i = 0; i < 7; ++i) expect(ln[i].size() <= 16, "help line fits the 16-character panel: " + ln[i]);
    }
    {
        controls::Bindings bb;
        controls::BindEditor ed;
        ed.begin(bb, controls::Coin, 0);
        static uint32_t c1[overlay::kOutW * overlay::kOutH], c2[overlay::kOutW * overlay::kOutH];
        for (auto &p : c1) p = 0xFFFFFF;
        for (auto &p : c2) p = 0xFFFFFF;
        draw_bindings(g, c1, ed, false);
        ed.select();
        draw_bindings(g, c2, ed, true);
        int lit = 0;
        for (uint32_t p : c1)
            if (p > 0x404040) ++lit;
        expect(lit > 800, "the bindings screen draws text");
        expect(std::memcmp(c1, c2, sizeof c1) != 0, "waiting for a key changes the screen");
        menu::Menu mm;
        mm.open();
        mm.key(Key::Down, Model());
        mm.key(Key::Down, Model());
        mm.key(Key::Down, Model());
        mm.key(Key::Down, Model());
        mm.key(Key::Enter, Model());
        expect(mm.page() == 2 && mm.visible_lines(Model())[0].find("BINDINGS") == 0, "Controls page starts with BINDINGS");
        mm.key(Key::Right, Model());
    }
    // S34B: pad hints in the menu and the help panel
    {
        Model pm;
        pm.pad_name[0] = "R3";
        pm.pad_name[2] = "LB";
        pm.pad_name[3] = "RB";
        pm.pad_name[4] = "Y";
        expect(value_text(Cmd::Resume, pm) == "R3" && value_text(Cmd::SaveState, pm) == "LB" && value_text(Cmd::LoadState, pm) == "RB",
               "the menu shows the pad button next to RESUME, SAVE STATE, LOAD STATE");
        Menu pmn;
        pmn.open();
        expect(pmn.visible_lines(pm)[0].find("R3") != std::string::npos && pmn.visible_lines(pm)[0].size() <= 24,
               "RESUME line carries the MENU button: " + pmn.visible_lines(pm)[0]);
        pm.purist = true;
        expect(pmn.visible_lines(pm)[1].find("LOCKED") != std::string::npos, "in Purist the SAVE STATE line says LOCKED, not the button");
        controls::Bindings pb;
        std::string pn[5];
        pad_names(pb, pn);
        expect(pn[0] == "R3" && pn[1].empty() && pn[4].empty(), "pad_names from the default bindings: " + pn[0]);
        static uint32_t h1[overlay::kOutW * overlay::kOutH], h2[overlay::kOutW * overlay::kOutH];
        for (auto &p : h1) p = 0;
        for (auto &p : h2) p = 0;
        Model m1, m2;
        m1.pad_type = PadType::Xbox;
        m2.pad_type = PadType::Xbox;
        m1.hints = 2;
        m2.hints = 2;
        m1.pad_btn[0] = 7;
        m2.pad_btn[0] = 9;
        draw_help(g, h1, m1);
        draw_help(g, h2, m2);
        expect(std::memcmp(h1, h2, sizeof h1) != 0, "the pad help panel shows the pad button");
    }
    // X09: the LIGHTBAR item
    {
        Model v;
        expect(value_text(Cmd::Lightbar, v) == "ON", "LIGHTBAR ON by default");
        v.lightbar = false;
        expect(value_text(Cmd::Lightbar, v) == "OFF", "LIGHTBAR OFF");
        v.lightbar = true;
        v.purist = true;
        expect(locked(Cmd::Lightbar, v), "LIGHTBAR is locked in Purist");
        Menu cm;
        cm.open();
        for (int i = 0; i < 4; ++i) cm.key(Key::Down, Model());
        cm.key(Key::Enter, Model());
        bool found = false;
        for (int i = 0; i < 12; ++i) {
            for (const std::string &l : cm.visible_lines(Model()))
                if (l.find("LIGHTBAR") == 0) {
                    found = true;
                    expect(l.size() <= 24, "LIGHTBAR line fits: " + l);
                }
            cm.key(Key::Down, Model());
        }
        expect(found, "the Controls page has a LIGHTBAR item");
        Settings a1;
        a1.lightbar = false;
        std::string err;
        const std::string sp = "/tmp/x09_settings.cfg";
        Settings a2;
        expect(save_settings(sp, a1, &err) && load_settings(sp, a2, &err) && !a2.lightbar, "the Lightbar setting is saved and loaded");
        Settings a3;
        expect(a3.lightbar, "Lightbar defaults to on");
    }
    // X22: RUN SUMMARY (Display page)
    { // X29: the achievements page shows a progress line for the highlighted, locked achievement that has a count
        Menu pm;
        pm.open();
        Model m;
        for (int i = 0; i < 7; ++i) pm.key(Key::Down, m);
        pm.key(Key::Enter, m);
        for (int i = 0; i < achievements::kFiveFruits; ++i) pm.key(Key::Down, m);
        static uint32_t a[overlay::kOutW * 300], b[overlay::kOutW * 300];
        const size_t N = sizeof a / sizeof *a;
        for (size_t i = 0; i < N; ++i) { a[i] = b[i] = 0; }
        m.ach_need[achievements::kFiveFruits] = 5;
        m.ach_have[achievements::kFiveFruits] = 2;
        pm.draw(g, a, m);
        m.ach_have[achievements::kFiveFruits] = 4;
        pm.draw(g, b, m);
        expect(std::memcmp(a, b, sizeof a) != 0, "the page shows the progress (2 of 5 and 4 of 5 look different)");
        Model m0;
        for (size_t i = 0; i < N; ++i) { b[i] = 0; }
        pm.draw(g, b, m0);
        expect(std::memcmp(a, b, sizeof a) != 0, "no count: no progress line");
        Model m1 = m;
        m1.ach_mask = 1ULL << achievements::kFiveFruits;
        for (size_t i = 0; i < N; ++i) { b[i] = 0; }
        pm.draw(g, b, m1);
        expect(std::memcmp(a, b, sizeof a) != 0, "an unlocked achievement shows no progress line");
    }
    {
        Model v;
        expect(value_text(Cmd::RunSummary, v) == "ON", "RUN SUMMARY ON by default");
        v.run_summary = false;
        expect(value_text(Cmd::RunSummary, v) == "OFF", "RUN SUMMARY OFF");
        v.run_summary = true;
        v.purist = true;
        expect(locked(Cmd::RunSummary, v) && value_text(Cmd::RunSummary, v) == "OFF", "RUN SUMMARY is locked and shows OFF in Purist");
        Menu dm;
        dm.open();
        for (int i = 0; i < 5; ++i) dm.key(Key::Down, Model());
        dm.key(Key::Enter, Model());
        bool found = false;
        for (const std::string &l : dm.visible_lines(Model()))
            if (l.find("RUN SUMMARY") == 0) {
                found = true;
                expect(l.size() <= 24, "RUN SUMMARY line fits: " + l);
            }
        expect(found && dm.page() == 3, "the Display page has a RUN SUMMARY item");
        Settings a1;
        a1.run_summary = false;
        std::string err;
        const std::string sp = "/tmp/x22_settings.cfg";
        Settings a2;
        expect(save_settings(sp, a1, &err) && load_settings(sp, a2, &err) && !a2.run_summary, "the RUN SUMMARY setting is saved and loaded");
        Settings a3;
        expect(a3.run_summary, "RUN SUMMARY defaults to on");
    }
    // X24: RECORDS page
    {
        Menu rm;
        Model v;
        rm.open();
        for (int i = 0; i < 6; ++i) rm.key(Key::Down, v);
        bool onit = false;
        for (const std::string &l : rm.visible_lines(v)) (void)l;
        onit = rm.visible_lines(v)[rm.cursor()] == "RECORDS";
        expect(onit, "the main page has a RECORDS item after DISPLAY");
        rm.key(Key::Enter, v);
        expect(rm.page() == 6, "Enter on RECORDS opens page 6");
        expect(rm.visible_lines(v).size() == 1 && rm.visible_lines(v)[0] == "BACK", "the Records page has only BACK as a menu row (the table is drawn separately)");
        rm.key(Key::Enter, v);
        expect(rm.page() == 0, "Enter on BACK leaves the Records page");
        rm.key(Key::Enter, v);
        rm.key(Key::Back, v);
        expect(rm.page() == 0, "Esc leaves the Records page");
        v.purist = true;
        Menu pm;
        pm.open();
        for (int i = 0; i < 6; ++i) pm.key(Key::Down, v);
        pm.key(Key::Enter, v);
        expect(pm.page() == 6, "the Records page can be looked at in Purist mode (it is only a view)");
    }
    // X25: ACHIEVEMENTS page
    {
        Menu am;
        Model v;
        am.open();
        for (int i = 0; i < 7; ++i) am.key(Key::Down, v);
        expect(am.visible_lines(v)[am.cursor()] == "ACHIEVEMENTS", "the main page has an ACHIEVEMENTS item after RECORDS");
        am.key(Key::Enter, v);
        expect(am.page() == 7, "Enter opens page 7");
        auto ls = am.visible_lines(v);
        expect((int)ls.size() == achievements::kCount + 1 && ls[0] == "POWER UP" && ls.back() == "BACK", "one row per achievement, then BACK");
        expect(am.key(Key::Enter, v).cmd == Cmd::None && am.page() == 7, "Enter on an achievement row does nothing (stays on the page)");
        for (int i = 0; i < achievements::kCount; ++i) { am.key(Key::Down, v); }
        expect(am.visible_lines(v)[am.cursor()] == "BACK", "Down reaches BACK");
        am.key(Key::Enter, v);
        expect(am.page() == 0, "Enter on BACK leaves the page");
        am.key(Key::Enter, v);
        am.key(Key::Back, v);
        expect(am.page() == 0, "Esc leaves the page");
        expect(am.item_at(100, 60) >= 0, "the main page rows still answer the mouse");
        am.key(Key::Enter, v);
        expect(am.item_at(100, 60) == -1, "page 7 (a table) does not answer the mouse");
        for (const std::string &l : ls) expect(l.size() <= 24, "row fits: " + l);
        for (int i = 0; i < achievements::kCount; ++i) {
            const achievements::Def &d = achievements::def(i);
            expect(std::strlen(d.name) <= 14 && std::strlen(d.key) > 0, std::string("achievement ") + d.key + ": name fits");
            for (const char *c = d.text; *c; ++c)
                expect((*c >= 'A' && *c <= 'Z') || *c == ' ' || *c == '.' || *c == ',' || (*c >= '0' && *c <= '9'),
                       std::string("description uses the game font's capitals: ") + d.text);
        }
    }
    // X08: which hint set is shown
    {
        Model h;
        h.pad_type = PadType::Xbox;
        h.hints = 0;
        h.hint_device = 1;
        expect(!hints_are_pad(h), "AUTO + keyboard = key caps");
        h.hint_device = 3;
        expect(hints_are_pad(h), "AUTO + pad = pad pictures");
        h.pad_type = PadType::None;
        expect(!hints_are_pad(h), "AUTO + pad but no pad connected = key caps");
        h.pad_type = PadType::PS4;
        h.hints = 1;
        expect(!hints_are_pad(h), "KEYBOARD forces key caps even after pad use");
        h.hints = 2;
        h.hint_device = 1;
        expect(hints_are_pad(h), "PAD forces pad pictures with a pad connected");
        h.pad_type = PadType::None;
        expect(!hints_are_pad(h), "PAD with no pad connected falls back to key caps");
        h.pad_type = PadType::PS5;
        h.hints = 2;
        h.purist = true;
        expect(!hints_are_pad(h), "Purist: key caps always");
        Model v;
        expect(value_text(Cmd::Hints, v) == "AUTO", "HINTS AUTO");
        v.hints = 1;
        expect(value_text(Cmd::Hints, v) == "KEYBOARD", "HINTS KEYBOARD");
        v.hints = 2;
        expect(value_text(Cmd::Hints, v) == "PAD", "HINTS PAD");
        expect(keycap_width("UP") == 15 && keycap_width("ESC") == 30 && keycap_width("F10") == 30 && keycap_width("A") == 14, "key cap widths");
        static uint32_t k1[overlay::kOutW * overlay::kOutH], k2[overlay::kOutW * overlay::kOutH];
        for (auto &p : k1) p = 0;
        for (auto &p : k2) p = 0;
        int sum = 0;
        const char *names[] = {"UP", "DOWN", "LEFT", "RIGHT", "ESC", "F1", "ENTER", "BKSP", "A"};
        for (const char *n : names) {
            for (auto &p : k1) p = 0;
            draw_keycap(g, k1, 20, 20, n);
            int lit = 0;
            for (uint32_t p : k1)
                if (p) ++lit;
            sum += lit;
            expect(lit > 40, std::string("key cap ") + n + " is drawn");
        }
        for (auto &p : k1) { p = 0; }
        draw_keycap(g, k1, 20, 20, "LEFT");
        draw_keycap(g, k2, 20, 20, "RIGHT");
        expect(std::memcmp(k1, k2, sizeof k1) != 0, "LEFT and RIGHT caps differ");
        (void)sum;
        controls::Bindings kb;
        std::string kc[7][2];
        binding_keycaps(kb, kc);
        expect(!kc[0][0].empty() && kc[4][0] == "5", "key caps from the default bindings: " + kc[4][0]);
        Settings st1;
        st1.hints = 2;
        std::string err;
        const std::string sp = "/tmp/x08_settings.cfg";
        Settings st2;
        expect(save_settings(sp, st1, &err) && load_settings(sp, st2, &err) && st2.hints == 2, "the Hints setting is saved and loaded");
    }
    {
        controls::Bindings bb;
        controls::BindEditor ed;
        ed.begin(bb, 11, 2);
        static uint32_t b1[overlay::kOutW * overlay::kOutH];
        for (auto &p : b1) p = 0xFFFFFF;
        draw_bindings(g, b1, ed, true);
        int lit = 0;
        for (uint32_t p : b1)
            if (p > 0x404040) ++lit;
        expect(lit > 800, "the 12-row bindings screen draws");
    }
    {
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
            std::string d = display_pad(b);
            expect(!d.empty() && d != "-" && d.size() <= 10, "pad button " + std::to_string(b) + " has a name that fits the 10-character column: " + d);
        }
        expect(display_pad(controls::kPadLeftTrigger) == "L2/LT" && display_pad(controls::kPadRightTrigger) == "R2/RT", "trigger names");
    }
    // X06: pause on focus loss: the menu item, the saved setting, Purist
    {
        Model fm;
        expect(value_text(Cmd::FocusPause, fm) == "ON" && label_of(Cmd::FocusPause) == "FOCUS LOSS PAUSE", "X06: on by default, labelled");
        expect(label_of(Cmd::FocusPause).size() + value_text(Cmd::FocusPause, fm).size() <= 24, "X06: the line fits the menu");
        fm.pause_focus = false;
        expect(value_text(Cmd::FocusPause, fm) == "OFF", "X06: off shows OFF");
        Menu fmn;
        fmn.open();
        for (int i = 0; i < 4; ++i) fmn.key(Key::Down, fm);
        fmn.key(Key::Enter, fm);
        bool found = false;
        for (const std::string &l : fmn.visible_lines(fm))
            if (l.find("FOCUS LOSS PAUSE") == 0) found = true;
        expect(found, "X06: the item is on the Controls page");
        for (int i = 0; i < 7; ++i) { fmn.key(Key::Down, fm); }
        expect(fmn.cursor() == 7 && fmn.key(Key::Enter, fm).cmd == Cmd::FocusPause, "X06: Enter on the item returns FocusPause");
        Model pm;
        pm.purist = true;
        expect(locked(Cmd::FocusPause, pm) && value_text(Cmd::FocusPause, pm) == "OFF", "X06: locked and OFF in Purist mode");
        Settings a;
        expect(a.pause_focus, "X06: setting default is on");
        a.pause_focus = false;
        std::string perr;
        expect(save_settings("build/s34/x06_settings.cfg", a, &perr), "X06: settings saved");
        Settings b;
        expect(load_settings("build/s34/x06_settings.cfg", b, &perr) && !b.pause_focus, "X06: setting round trip off");
        a.pause_focus = true;
        save_settings("build/s34/x06_settings.cfg", a, &perr);
        Settings c;
        c.pause_focus = false;
        load_settings("build/s34/x06_settings.cfg", c, &perr);
        expect(c.pause_focus, "X06: setting round trip on");
    }
    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
