// S22/S24: the Pac-Man port in a window. Runs the C++ machine (src/board) and draws it with the tile and sprite renderers (src/video)
// at the arcade rate of 60.606 Hz. Keyboard and gamepad play it (S24). Sound plays live through the sound card (S26).
//
//   pacman-port [--rom PATH] [--scale N] [--dump N OUT.png [--tiles-only | --no-mame-sprites]] [--measure SECONDS] [--report FILE]
//               [--fit] [--scanlines] [--borderless] [--overlay] [--pacing exact|smooth] [--ticks N] [--sim-refresh HZ] [--keys F3@1000,...]
//
//   --rom PATH        the player's own pacman.zip. Without it the program looks next to itself, then in roms/, then in the current folder.
//   --scale N         first window size = N x the arcade picture (default 3). The picture is always drawn at a whole-number size.
//   --dump N OUT.png  no window: run N frames, save the picture of frame N as a PNG, exit.
//                     --tiles-only draws only the tile layer, --no-mame-sprites uses the old prototype sprite rules (both are for checking against earlier steps).
//   --measure SECONDS open the window, run for that long, report the measured machine-tick rate, exit.
//   --autoplay SECONDS  no window, no real-time wait: coin at frame 400, start at 420, then SECONDS of scripted joystick play through the same
//                     Inputs path as the keyboard; reports what happened (self-test for S24). Add --dump-end FILE.png to save the last picture.
//   --scripted        in the window, play the scripted game of --autoplay instead of reading keyboard and pad (for unattended tests)
//   --no-audio        do not open the sound card
//   --audio-capture FILE.wav  also save exactly what was handed to the sound card (checks the live path; keep runs under about 2 minutes)
//   --report FILE     also write the text messages to FILE (a program built for Windows has no console).
//   --fit             fill the window (aspect kept, any size) instead of whole-number sizes only. --scanlines: dark lines between the arcade's pixel columns
//                     (the monitor stood on its side, so its scan lines run up and down in the upright picture). --borderless: no window frame.
//   --overlay         start with the debug overlay on (F3 toggles it; it only reads memory and adds a 128-pixel panel to the right of the picture).
//   --pacing smooth   one machine tick every N display refreshes when N refreshes are within 3 % of the arcade rate (180 Hz: N = 3 = 60.000 ticks/s,
//                     1.00 % slower than 60.606), so every picture is shown equally long. exact (default): 60.606 ticks per second, some pictures stay one refresh longer.
//   --ticks N         stop after exactly N machine ticks and print a hash of the machine's memory (the same N ticks must give the same hash whatever the options).
//   --sim-refresh HZ  pretend the screen refreshes at HZ (for pacing tests where no real screen is available); --keys: press keys at tick numbers (F3, F5, F6, F7, F8, F11).
//   S33: F1 = pause / resume, F2 = quick save (also written to quicksave.sav next to controls.cfg), F4 = quick load, hold Backspace = rewind (the last 30 seconds).
//        The high score is kept in highscore.txt next to controls.cfg: put into the game once when the attract screen first appears, written when a game ends
//        with a record. After F4 the saved high score is the one the state had when it was saved; scores made after a load or rewind never count. --no-hiscore turns the file off, --hiscore FILE moves it.
//   S34 (src/app/menu.*): Esc = pause and open the menu (Resume, Save/Load state, Game options, Controls, Display, Purist mode, Restart, Quit); the menu and the
//         function keys do the same things, and a key-help panel on the right lists every key. Game options (lives, bonus life, difficulty, ghost names) are the ROM's
//         own DIP switches: they are read at power-on, so "Apply - restart" (or Restart game) restarts the machine. Purist mode (menu, or --purist) switches off every
//         addition (overlay, scanlines, fill scaling, smooth pacing, one-button start, save states, rewind, saved high score) and restarts the machine. Settings are kept in
//         settings.cfg next to controls.cfg. Test aids: --replay FILE plays a recorded input file (tools/replay format), --hashlog FILE writes "frame sha1", --fast = no waiting.
//   T02a: --rules readable runs the readable rules (src/rules/rules.h, src/logic) instead of the original ROM routines (Purist mode ignores it).
//   S34A (controls.h BindEditor, menu.cpp draw_bindings): Controls > Bindings (also F9, and F10 which starts on the pad column) opens a table of every command
//         with key 1, key 2 and pad button: arrows move, Enter then a key changes only that slot (a key already in use is swapped with the other slot and the screen says so),
//         Backspace clears a slot, Esc cancels or goes back. The d-pad and left stick always move (shown as fixed). Changes are saved at once in controls.cfg.
//         The old step-by-step dialogue (Remap in controls.cpp) is no longer used by the program. The key-help panel shows the keys as they are now.
//   S34B: five system commands can also be done from the pad (Bindings screen, pad column): MENU (default: right stick click; the same button closes the menu),
//         PAUSE, QUICK SAVE, QUICK LOAD, REWIND (hold); the last four start unset. With the menu open the pad works it fully (d-pad, A select, B back), so Save state and Load state
//         are reachable by pad without any extra binding. Purist mode locks save, load and rewind on the pad too. Test aid: --pad-buttons MS:BUTTON,... pushes real SDL pad-button events
//         (BUTTON = SDL name such as rightstick, dpdown, a; a leading - releases the button) MS milliseconds after start.
//   Keys: arrows or W A S D move, 5 inserts a coin, 1 starts one player, 2 starts two players, Esc opens the menu (in --scripted runs it closes the window), F11 toggles full screen,
//         F3 debug overlay, F5 scanlines, F6 pacing exact/smooth, F7 window frame on/off, F8 whole-number / fill scaling.
//   Gamepad: d-pad or left stick moves, Back/Select/Share inserts a coin, Start/Options starts one player.
//   S32 controls (src/app/controls.*): F9 = change the keys (the window title asks for each key in turn; Esc cancels), F10 = change the pad buttons
//         (coin, start 1, start 2; Backspace = none), F12 = back to the standard controls. The choices are kept in controls.cfg in your AppData folder
//         (Windows: %APPDATA%\PacmanPort\controls.cfg) and can also be edited with Notepad. One-button Start (on by default, line one_button_start in
//         the file): pressing Start in the attract screen with no credit puts the coin in first, then starts. --config FILE uses another settings file.
//
// Build: tools/build_app.sh (native) and the app line at the end of tools/build_windows.sh (Windows).
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "../board/machine.h"
#include "../game/output.h"
#include "../board/romset.h"
#include "../board/inputs.h"
#include "../audio/wsg.h"
#include "../audio/audio_out.h"
#include "../board/inputs.cpp" // included here, not compiled separately: S24 and S26 may only touch src/app/ (and src/audio/), so tools/build_*.sh could not be given more source files
#include "../audio/wsg.cpp"
#include "../audio/audio_out.cpp"
#include "../video/gfx.h"
#include "../third_party/miniz/miniz_tdef.h"
#include "haptics.h"
#include "lastdevice.h"   // X07: which device was used last
#include "events.h"       // X01: game event reader (reads memory only)
#include "achievements.h" // X25: local achievements (reads events only)
#include "crt.h"          // X40: the CRT shadow mask (drawn over the scaled picture; never touches the emulated picture)
#include "firstrun.h"     // S35: first-run ROM finder
#include "surround.h"     // X44: the background layer around the picture
#include "records.h"      // X24: personal bests per level
#include "stats.h"        // X22: per-run statistics (reads events only)
#include "overlay.cpp"    // included, not compiled separately (this step may not change tools/build_*.sh)
#include "display.cpp"
#include "controls.cpp"
#include "state.cpp"
#include "replay.cpp"       // S34: the per-frame hash used by --replay / --hashlog
#include "lightbar.h"       // X09
#include "menu.cpp"         // S34: pause menu, help panel, settings, Purist rules
#include "cheat.h" // the INVINCIBLE test cheat (writes to the machine on purpose; an assist)
#include "dipnow.h"          // S34D: new DIP settings put into a running machine at a quiet moment
#include "../rules/rules.h" // T02a: the readable rules in the game (only with --rules readable)

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out);
void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules);

static const int W = 224, H = 288;
static const double TICK_HZ = 3072000.0 / 50688.0; // 60.60606 Hz: CPU clock divided by cycles per frame (hardware_facts.md section 1)

static std::string g_report_path;
static std::string g_report;
static void say(const std::string &s) {
    std::printf("%s\n", s.c_str());
    std::fflush(stdout);
    g_report += s + "\n";
}
static void write_report() {
    if (g_report_path.empty()) return;
    FILE *f = std::fopen(g_report_path.c_str(), "wb");
    if (f) {
        std::fwrite(g_report.data(), 1, g_report.size(), f);
        std::fclose(f);
    }
}

#ifdef PAC_NO_Z80
// N49c: an internal error stops the program; the message must reach the log file first (the report file if one was asked for, else crash-report.txt next to the program)
static void on_fatal(const char *msg) {
    g_report += std::string("FATAL: ") + msg + "\nThe game stopped because of an internal error. Please send this file.\n";
    if (g_report_path.empty()) g_report_path = "crash-report.txt";
    write_report();
}
#endif
static bool save_png_sized(const std::string &path, const uint32_t *img, int w, int h);
static bool save_png(const std::string &path, const uint32_t *img) { return save_png_sized(path, img, W, H); }
static bool save_png_sized(const std::string &path, const uint32_t *img, int W, int H) {
    std::vector<uint8_t> rgb(W * H * 3);
    for (int i = 0; i < W * H; ++i) {
        rgb[3 * i] = img[i] >> 16;
        rgb[3 * i + 1] = img[i] >> 8;
        rgb[3 * i + 2] = img[i];
    }
    size_t n = 0;
    void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), W, H, 3, &n);
    if (!png) return false;
    FILE *f = std::fopen(path.c_str(), "wb");
    bool ok = f && std::fwrite(png, 1, n, f) == n;
    if (f) std::fclose(f);
    std::free(png); // miniz allocates with malloc by default
    return ok;
}

struct Options {
    std::string config; // S32: settings file (default: the AppData folder)
    std::string rom, dump_path, exe_dir;
    bool no_dialog = false;
    int scale = 3, dump_frame = 0;
    double measure = 0, autoplay = 0;
    std::string dump_end;
    bool tiles_only = false, mame_sprites = true, scripted = false, no_audio = false;
    std::string audio_capture;
    int crt_mask = -1, glow = -1, curve = -1, surround_mode = -1;
    bool fit = false, scanlines = false, borderless = false, overlay_on = false, smooth = false;
    std::string hiscore;
    bool no_hiscore = false; // S33: high-score file (default: next to the controls file)
    std::string
        burst; // S36 test aid: "TICK:MS" - from that machine tick for MS milliseconds the simulated screen does not wait for its refresh (what a window resize does to the Direct3D screen timing on Windows)
    std::string stall; // test aid: "TICK:MS[,TICK:MS...]" freezes the program for MS milliseconds at that machine tick (what a slow pad unplug does on Windows)
    long ticks_limit = 0;
    double sim_refresh = 0;
    std::string keys;
    std::string cmd_at;   // S34D test aid: TICK:NAME,... runs a menu command at a machine tick (purist, lives, bonus, difficulty, names, overlay, scanlines, rules, surround, restart)
    std::string shot, events_file, stats_file, test_clears, replay, hashlog, pad_buttons, pad_virtual;
    bool purist = false, fast = false, virtual_pad = false;
    std::string rules_file, write_rules; // T02i: --rules-file PATH (a rules file for this run, also in replays), --write-rules-file PATH (write the standard file and stop)
    bool rules_readable = false; // T02a: --rules readable runs the readable rules (src/rules) instead of the ROM routines; never in Purist mode
    std::string virtual_type;
    int vibration = -1,
        pad_symbols =
            -1; // virtual_pad / pad_virtual: test aid, a pretend standard pad (SDL virtual joystick) driven by "MS:aN=VALUE" (axis N) and "MS:bN=0|1" (button N) in SDL's virtual layout: a0..a3 sticks, a4 = left trigger, a5 = right trigger, b0 A, b1 B, b2 X, b3 Y, b4 back, b5 guide, b6 start, b7 L3, b8 R3, b9 LB, b10 RB, b15 misc1, b20 touchpad;   // pad_buttons: test aid "MS:BUTTON[,...]" (BUTTON = SDL name such as rightstick; a leading - releases it)
                // S34: --replay FILE plays a recorded input file, --hashlog FILE writes "frame sha1" like tools/replay, --purist, --fast (no waiting)
};

static void compose(const Gfx &g, const RomSet &roms, const Machine &m, const Options &o, uint32_t *fb) {
    game::Output out = game::output_view(const_cast<Machine &>(m).bus);   // N53: the named output of the game (same bytes as before)
    render_tilemap(g, out.screen.tile, out.screen.colour, fb);
    if (!o.tiles_only) render_sprites(g, roms.palettes.data(), (const uint8_t *)&out.sprites, (const uint8_t *)&out.coords, fb, o.mame_sprites);
}

int main(int argc, char **argv) {
#ifdef PAC_NO_Z80
    fatal_hook = on_fatal;   // N49c
#endif
    Options o;
    if (char *base = SDL_GetBasePath()) {
        o.exe_dir = base;
        SDL_free(base);
    }
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) o.rom = argv[++i];
        else if (a == "--scale" && i + 1 < argc) o.scale = std::atoi(argv[++i]);
        else if (a == "--dump" && i + 2 < argc) {
            o.dump_frame = std::atoi(argv[++i]);
            o.dump_path = argv[++i];
        } else if (a == "--measure" && i + 1 < argc) o.measure = std::atof(argv[++i]);
        else if (a == "--scripted") o.scripted = true;
        else if (a == "--no-audio") o.no_audio = true;
        else if (a == "--audio-capture" && i + 1 < argc) o.audio_capture = argv[++i];
        else if (a == "--autoplay" && i + 1 < argc) o.autoplay = std::atof(argv[++i]);
        else if (a == "--dump-end" && i + 1 < argc) o.dump_end = argv[++i];
        else if (a == "--report" && i + 1 < argc) g_report_path = argv[++i];
        else if (a == "--fit") o.fit = true;
        else if (a == "--scanlines") o.scanlines = true;
        else if (a == "--shot" && i + 1 < argc)
            o.shot = argv[++i]; // X40 test aid: with --ticks, the window picture (what the player sees, mask included) is written as a PNG on the last tick
        else if (a == "--no-dialog") o.no_dialog = true; // S35: never show the ROM message boxes
        else if (a == "--surround" && i + 1 < argc) o.surround_mode = surround::from_name(argv[++i]);
        else if (a == "--curve" && i + 1 < argc) o.curve = crt::from_name(argv[++i]);
        else if (a == "--glow" && i + 1 < argc) o.glow = crt::from_name(argv[++i]);
        else if (a == "--crt-mask" && i + 1 < argc) o.crt_mask = crt::from_name(argv[++i]);
        else if (a == "--borderless") o.borderless = true;
        else if (a == "--overlay") o.overlay_on = true;
        else if (a == "--pacing" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "smooth") o.smooth = true;
            else if (v != "exact") {
                say("--pacing must be exact or smooth");
                write_report();
                return 2;
            }
        } else if (a == "--ticks" && i + 1 < argc) o.ticks_limit = std::atol(argv[++i]);
        else if (a == "--sim-refresh" && i + 1 < argc) o.sim_refresh = std::atof(argv[++i]);
        else if (a == "--keys" && i + 1 < argc) o.keys = argv[++i];
        else if (a == "--cmd-at" && i + 1 < argc) o.cmd_at = argv[++i];
        else if (a == "--config" && i + 1 < argc) o.config = argv[++i];
        else if (a == "--burst" && i + 1 < argc) o.burst = argv[++i];
        else if (a == "--stall" && i + 1 < argc) o.stall = argv[++i];
        else if (a == "--replay" && i + 1 < argc) o.replay = argv[++i];
        else if (a == "--hashlog" && i + 1 < argc) o.hashlog = argv[++i];
        else if (a == "--test-clear" && i + 1 < argc)
            o.test_clears = argv
                [++i]; // X24: test aid "LEVEL,SCORE,FRAMES[;...]": pretends those levels were cleared (one every 120 machine frames from frame 400); the machine is not touched
        else if (a == "--stats" && i + 1 < argc)
            o.stats_file = argv[++i]; // X22: test aid, writes the per-run statistics of every finished game (and the games still running at exit)
        else if (a == "--events" && i + 1 < argc) o.events_file = argv[++i]; // X01: test aid, writes every game event as a text line
        else if (a == "--purist") o.purist = true;
        else if (a == "--rules" && i + 1 < argc) o.rules_readable = std::string(argv[++i]) == "readable";
        else if (a == "--rules-file" && i + 1 < argc) o.rules_file = argv[++i];
        else if (a == "--write-rules-file" && i + 1 < argc) o.write_rules = argv[++i];
        else if (a == "--pad-buttons" && i + 1 < argc) o.pad_buttons = argv[++i];
        else if (a == "--virtual-pad") o.virtual_pad = true;
        else if (a == "--vibration" && i + 1 < argc) {
            std::string v = argv[++i];
            o.vibration = v == "off" ? 0 : v == "light" ? 1 : 2;
        } else if (a == "--pad-symbols" && i + 1 < argc) {
            std::string v = argv[++i];
            o.pad_symbols = v == "xbox" ? 1 : v == "ps4" ? 2 : v == "ps5" ? 3 : 0;
        } else if (a == "--virtual-type" && i + 1 < argc) {
            o.virtual_type = argv[++i];
            o.virtual_pad = true;
        } else if (a == "--pad-virtual" && i + 1 < argc) {
            o.pad_virtual = argv[++i];
            o.virtual_pad = true;
        } else if (a == "--fast") o.fast = true;
        else if (a == "--hiscore" && i + 1 < argc) o.hiscore = argv[++i];
        else if (a == "--no-hiscore") o.no_hiscore = true;
        else if (a == "--tiles-only") o.tiles_only = true;
        else if (a == "--no-mame-sprites") o.mame_sprites = false;
        else {
            say("Unknown option: " + a);
            write_report();
            return 2;
        }
    }
    if (o.scale < 1) o.scale = 1;
    if (!o.write_rules.empty()) { // T02i: write the standard rules file for modders and stop
        std::string e;
        if (!rules::write_standard_file(o.write_rules, &e)) {
            say("Rules file: " + e);
            write_report();
            return 2;
        }
        say("Rules file: wrote the standard rules to " + o.write_rules);
        write_report();
        return 0;
    }

    { // S35: find the ROM (next to the program, a roms folder, the current folder, the place chosen last time), else a friendly message and a file picker (not in scripted or measuring runs)
        const std::vector<std::string> cands = {o.exe_dir + "pacman.zip", o.exe_dir + "roms/pacman.zip", "pacman.zip", "roms/pacman.zip"};
        const bool interactive = !o.no_dialog && g_report_path.empty() && o.dump_frame <= 0 && o.measure <= 0 && o.replay.empty() && o.ticks_limit <= 0 &&
                                 o.shot.empty() && o.hashlog.empty() && !o.scripted;
        firstrun::Ui ui = firstrun::system_ui();
        const std::string cfg0 = o.config.empty() ? controls::default_config_path() : o.config;
        const std::string sdir = cfg0.empty() ? std::string() : std::filesystem::path(cfg0).parent_path().string();
        if (!firstrun::resolve(o.rom, cands, sdir, interactive ? &ui : nullptr, [&](const std::string &t) { say("ROM: " + t); })) {
            write_report();
            return 2;
        }
        say("ROM: using " + o.rom);
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(o.rom, roms, err)) {
        say("The ROM zip was not accepted:\n" + err);
        write_report();
        return 2;
    }
    static Gfx g;
    if (!decode_gfx(roms, g)) {
        say("The graphics ROMs could not be decoded.");
        write_report();
        return 2;
    }
    Machine m(roms.program);
    rules::standard_only() = !(o.rules_readable && !o.purist);
    m.rules = rules::table_for(o.rules_readable && !o.purist); // T02a (N49b: without the Z80 the table always runs)
    rules::user_tuning().reset();
    if (!o.rules_file.empty()) { // T02i: a rules file given on the command line (also for replays and tests)
        std::string pr, er;
        if (!rules::load_file(o.rules_file, rules::user_tuning(), &pr, &er)) say("Rules file: " + er);
        else {
            say("Rules file: loaded " + o.rules_file + (rules::user_tuning().active ? " (values differ from the standard: MODDED)" : " (all values standard)"));
            if (!pr.empty()) say("Rules file: some lines were ignored:\n" + pr);
        }
        if (rules::user_tuning().active && !o.purist) { rules::standard_only() = false; m.rules = rules::table_for(true); }
    }
    static uint32_t fb[W * H];

    // ---- --dump: no window at all
    if (o.dump_frame > 0) {
        for (int i = 0; i < o.dump_frame; ++i) m.run_frame();
        compose(g, roms, m, o, fb);
        if (!save_png(o.dump_path, fb)) {
            say("Could not write " + o.dump_path);
            write_report();
            return 1;
        }
        say("Saved frame " + std::to_string(m.frame) + " to " + o.dump_path);
        write_report();
        return 0;
    }

    // ---- --autoplay: scripted game, no window
    if (o.autoplay > 0) {
        Inputs in;
        const int play_from = 424, total = play_from + (int)(o.autoplay * TICK_HZ);
        uint32_t rng = 12345;
        int hold = 0;
        Control dir = Control::P1Left;
        const Control dirs[4] = {Control::P1Up, Control::P1Left, Control::P1Right, Control::P1Down};
        int ready_frame = -1, game_frame = -1, state_changes = 0, last_state = -1;
        for (int f = 0; f < total; ++f) {
            in.set(Control::Coin1, f >= 400 && f < 405);
            in.set(Control::Start1, f >= 420 && f < 424);
            for (Control c : dirs) in.set(c, false);
            if (f >= 430) {
                if (hold <= 0) {
                    rng = rng * 1664525u + 1013904223u;
                    dir = dirs[(rng >> 24) & 3];
                    hold = 10 + (int)((rng >> 16) & 31);
                }
                --hold;
                in.set(dir, true);
            }
            in.apply(m.bus);
            m.run_frame();
            int st = m.bus.read(0x4E00);
            if (st != last_state) {
                ++state_changes;
                last_state = st;
            }
            if (st == 3 && f >= 420 && game_frame < 0) game_frame = f;
        }
        (void)ready_frame;
        compose(g, roms, m, o, fb);
        if (!o.dump_end.empty() && !save_png(o.dump_end, fb)) say("Could not write " + o.dump_end);
        auto r = [&](int a) { return (int)m.bus.read(a); };
        int score = 0;
        for (int i = 2; i >= 0; --i) {
            int b = r(0x4E80 + i);
            score = score * 100 + (b >> 4) * 10 + (b & 15);
        }
        char line[200];
        std::snprintf(line, sizeof line, "Autoplay: %d frames run (%.1f s of play after start). Game started at frame %d.", total, (total - play_from) / TICK_HZ,
                      game_frame);
        say(line);
        std::snprintf(line, sizeof line, "End state: main state %d, level state %d, lives %d, dots eaten %d, score %d, credits %d.", r(0x4E00), r(0x4E04), r(0x4E14),
                      r(0x4E0E), score, r(0x4E6E));
        say(line);
        bool ok = game_frame >= 0 && r(0x4E0E) > 0;
        say(ok ? "PASS autoplay: the game started and Pac-Man ate dots under joystick control." : "FAIL autoplay: the game did not start or Pac-Man never moved.");
        write_report();
        return ok ? 0 : 1;
    }

    // ---- S34: saved settings and the replay (never in scripted or replay runs: those must be the plain machine)
    namespace fsx = std::filesystem;
    const bool use_settings = !o.scripted && o.replay.empty();
    const std::string cfg_path = o.config.empty() ? controls::default_config_path() : o.config;
    const std::string settings_path = cfg_path.empty() ? std::string() : (fsx::path(cfg_path).parent_path() / "settings.cfg").string();
    menu::Settings st;
    if (use_settings) {
        std::string pr;
        if (menu::load_settings(settings_path, st, &pr)) {
            say("Settings: loaded " + settings_path);
            if (!pr.empty()) say("Settings: some lines were ignored:\n" + pr);
        } else say("Settings: no settings file yet (" + (settings_path.empty() ? std::string("no settings folder") : settings_path) + "), using the standard settings");
    }
    bool purist = o.purist, help_pin = st.help_pin;   // S34D: `purist` is only the test flag --purist now (Purist mode in the menu is the Reset to defaults button)
    bool rules_pref = o.rules_readable || st.rules_readable; // T02i: RULES READABLE chosen (command line or settings)
    if (use_settings && o.rules_file.empty()) { // T02i: rules.cfg next to the program, else in the settings folder
        const std::string settings_dir = settings_path.empty() ? std::string() : fsx::path(settings_path).parent_path().string();
        const std::string cands[2] = {o.exe_dir + "rules.cfg", settings_dir.empty() ? std::string() : (fsx::path(settings_dir) / "rules.cfg").string()}; // next to the program first
        for (const std::string &c : cands) {
            std::error_code ec;
            if (c.empty() || !fsx::exists(c, ec)) continue;
            std::string pr, er;
            if (rules::load_file(c, rules::user_tuning(), &pr, &er)) {
                say("Rules file: loaded " + c + (rules::user_tuning().active ? " (values differ from the standard: MODDED" + std::string(purist ? ", but Purist mode ignores it)" : ")") : " (all values standard)"));
                if (!pr.empty()) say("Rules file: some lines were ignored:\n" + pr);
                break;
            }
        }
    }
    auto rules_readable_now = [&]() { return !purist && (rules_pref || rules::user_tuning().active); };
    auto rules_table = [&]() -> const Machine::RuleHook * { rules::standard_only() = !rules_readable_now(); return rules::table_for(rules_readable_now()); }; // T02i (N49b: without the Z80 the original rules are the same table with the standard tuning)
    m.rules = rules_table(); // T02a: Purist mode always runs the original ROM routines
    say(rules_readable_now() ? (rules::user_tuning().active ? "Game rules: READABLE with a rules file (MODDED)" : "Game rules: READABLE (the proved readable rules in src/logic run instead of the ROM routines)") : "Game rules: original ROM routines"
#ifdef PAC_NO_Z80
        " (run as native code, proved equal to the ROM frame by frame)"
#endif
        );
    o.overlay_on = o.overlay_on || st.overlay;
    o.scanlines = o.scanlines || st.scanlines;
    o.fit = o.fit || st.fill;
    o.smooth = o.smooth || st.smooth;
    o.borderless = o.borderless || !st.frame;
    DipSettings dip_chosen;
    dip_chosen.lives = (DipSettings::Lives)st.lives;
    dip_chosen.bonus = (DipSettings::Bonus)st.bonus;
    dip_chosen.normal_difficulty = st.diff_normal;
    dip_chosen.normal_ghost_names = st.names_normal;
    DipSettings applied_dip = dip_chosen; // what the machine was started with (the ROM reads the DIP switches only at power-on)
    // S34E: achievements count only while every GAME setting is at its standard value (what the running game really uses: the DIP switches the machine sees, m.bus.dsw1, and the rules).
    auto game_settings_standard = [&]() -> bool {
        const menu::Settings d;
        DipSettings x;
        x.lives = (DipSettings::Lives)d.lives;
        x.bonus = (DipSettings::Bonus)d.bonus;
        x.normal_difficulty = d.diff_normal;
        x.normal_ghost_names = d.names_normal;
        return m.bus.dsw1 == x.to_byte() && rules_pref == d.rules_readable && !rules::user_tuning().active;
    };
    bool ach_settings_said = true;   // the last state of game_settings_standard() that was written to the log
    Replay rp;
    bool replay_on = false;
    FILE *hashlog = nullptr;
    uint32_t replay_frame = 0;
    if (!o.replay.empty()) {
        std::string re;
        if (!rp.load(o.replay, re)) {
            say("Replay: " + re);
            write_report();
            return 2;
        }
        replay_on = true;
        o.no_audio = true;
        say("Replay: " + o.replay + " (" + std::to_string(rp.frames) + " frames)");
    }
    if (!o.hashlog.empty()) {
        hashlog = std::fopen(o.hashlog.c_str(), "wb");
        if (!hashlog) {
            say("Cannot write " + o.hashlog);
            write_report();
            return 2;
        }
    }
    say(purist ? "Purist mode: ON (no overlay, filters, one-button start, save states, rewind or saved high score)" : "Purist mode: off");
    const bool panel0 = (o.overlay_on && !purist) || help_pin;

    // ---- window
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
        say(std::string("SDL could not start: ") + SDL_GetError());
        write_report();
        return 1;
    }
    SDL_DisplayMode dm;
    int sc = o.scale;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
        while (sc > 1 && (W * sc > dm.w || H * sc > dm.h - 80)) --sc; // keep the first window on the screen
    SDL_Window *win = SDL_CreateWindow("PacArcadeComp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (panel0 ? overlay::kOutW : W) * sc, H * sc,
                                       SDL_WINDOW_RESIZABLE | (o.borderless ? SDL_WINDOW_BORDERLESS : 0));
    if (!win) {
        say(std::string("Could not open a window: ") + SDL_GetError());
        write_report();
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        say(std::string("Could not create a renderer: ") + SDL_GetError());
        write_report();
        return 1;
    }
    SDL_RendererInfo ri;
    SDL_GetRendererInfo(ren, &ri);
    const bool vsync = (ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    // The picture is placed by display::compute_layout (whole-number scale by default, or fill); two textures hold the same pixels, one filtered nearest (sharp
    // whole-number sizes) and one linear (used only for a fill size that is not a whole number).
    const int TW = overlay::kOutW, TH = overlay::kOutH;
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "nearest", SDL_HINT_OVERRIDE);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TW, TH);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear", SDL_HINT_OVERRIDE);
    SDL_Texture *texl = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, TW, TH);
    say(std::string("Renderer: ") + ri.name + (vsync ? " (vsync on)" : " (vsync off)") + ", first window " + std::to_string((panel0 ? overlay::kOutW : W) * sc) + "x" +
        std::to_string(H * sc));
    double refresh_hz = 0;
    {
        SDL_DisplayMode cur;
        int di = SDL_GetWindowDisplayIndex(win);
        if (di >= 0 && SDL_GetCurrentDisplayMode(di, &cur) == 0) refresh_hz = cur.refresh_rate;
    }
    if (o.sim_refresh > 0) refresh_hz = o.sim_refresh;
    say("Display refresh rate: " +
        (refresh_hz > 0 ? std::to_string((int)(refresh_hz + 0.5)) + " Hz" + (o.sim_refresh > 0 ? " (simulated)" : "") : std::string("unknown")));

    // ---- gamepad: a standard pad (PS4, Xbox...) through SDL's game controller layer; anything else through the plain joystick layer
    SDL_GameController *pad = nullptr;
    SDL_Joystick *joy = nullptr;
    std::string padName;
    auto openPads = [&]() {
        for (int i = 0; i < SDL_NumJoysticks() && !pad && !joy; ++i) {
            if (SDL_IsGameController(i)) {
                pad = SDL_GameControllerOpen(i);
                if (pad) padName = SDL_GameControllerName(pad);
            } else {
                joy = SDL_JoystickOpen(i);
                if (joy) padName = std::string(SDL_JoystickName(joy)) + " (not a standard pad)";
            }
        }
    };
    controls::PadAnnouncer announcer;
    auto announce_pad = [&](const char *prefix) {
        if (!padName.empty() && announcer.should_announce(padName, (long)SDL_GetTicks())) say(std::string(prefix) + padName);
    };
    static std::atomic<int> vl_calls{0}, vl_last[3] = {{0}, {0}, {0}};                                    // X09: what the pretend pad's light bar was told
    static std::atomic<int> vr_calls{0}, vr_trig_calls{0}, vr_low_max{0}, vr_high_max{0}, vr_trig_max{0}; // what the pretend pad was asked to do (test aid)
    // S34C: the kind of pad in use (for the icon and the button pictures) and a short notice when a pad is plugged in or removed
    int pad_symbols = o.pad_symbols >= 0 ? o.pad_symbols : st.pad_symbols; // 0 auto, 1 Xbox, 2 PS4, 3 PS5: which button pictures to draw (the player can force them)
    auto pad_type_now = [&]() {
        if (!pad && !joy) return menu::PadType::None;
        if (pad_symbols == 1) { return menu::PadType::Xbox; }
        if (pad_symbols == 2) { return menu::PadType::PS4; }
        if (pad_symbols == 3) { return menu::PadType::PS5; }
        return pad ? menu::pad_type_of_sdl((int)SDL_GameControllerGetType(pad), true) : menu::PadType::Other;
    };
    auto list_pads = [&]() {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            char b[300];
            std::snprintf(b, sizeof b, "Pads seen: #%d \"%s\" type %d vendor:product %04X:%04X%s", i, SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "?",
                          (int)SDL_GameControllerTypeForIndex(i), SDL_JoystickGetDeviceVendor(i), SDL_JoystickGetDeviceProduct(i),
                          SDL_IsGameController(i) ? "" : " (not a standard pad)");
            say(b);
        }
    };
    menu::PadType toast_type = menu::PadType::None;
    bool toast_connected = false;
    uint32_t toast_until = 0;
    auto toast = [&](bool connected, menu::PadType t) {
        toast_type = t;
        toast_connected = connected;
        toast_until = SDL_GetTicks() + 3500;
        say(std::string("Gamepad icon: ") + menu::pad_type_word(t) + (connected ? " connected" : " removed"));
    };
    if (o.virtual_pad) { // test aid: a pretend standard pad (SDL gives a virtual joystick of this type the standard layout: sticks a0-a3, triggers a4 and a5, buttons b0-b20)
        SDL_VirtualJoystickDesc vd;
        SDL_zero(vd);
        vd.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        vd.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        vd.naxes = 6;
        vd.nbuttons = 21;
        vd.name = "Virtual test pad";
        if (o.virtual_type == "xbox360") {
            vd.vendor_id = 0x045e;
            vd.product_id = 0x028e;
        } else if (o.virtual_type == "xboxone") {
            vd.vendor_id = 0x045e;
            vd.product_id = 0x02ea;
        } else if (o.virtual_type == "ps4") {
            vd.vendor_id = 0x054c;
            vd.product_id = 0x09cc;
        } else if (o.virtual_type == "ps5") {
            vd.vendor_id = 0x054c;
            vd.product_id = 0x0ce6;
        }
        vd.Rumble = [](void *, Uint16 lo, Uint16 hi) -> int {
            ++vr_calls;
            if (lo > vr_low_max) vr_low_max = lo;
            if (hi > vr_high_max) vr_high_max = hi;
            return 0;
        };
        if (o.virtual_type == "ps4" || o.virtual_type == "ps5")
            vd.SetLED = [](void *, Uint8 r, Uint8 g, Uint8 b) -> int {
                ++vl_calls;
                vl_last[0] = r;
                vl_last[1] = g;
                vl_last[2] = b;
                return 0;
            }; // X09: only the PlayStation pretend pads have a light bar
        vd.RumbleTriggers = [](void *, Uint16 l, Uint16 r) -> int {
            ++vr_trig_calls;
            int v = std::max<int>(l, r);
            if (v > vr_trig_max) vr_trig_max = v;
            return 0;
        };
        if (SDL_JoystickAttachVirtualEx(&vd) < 0) say(std::string("Test pad: could not create (") + SDL_GetError() + ")");
        else say("Test pad: a virtual standard pad is connected");
    }
    openPads();
    list_pads();
    announce_pad("Gamepad: ");
    if (pad || joy) toast(true, pad_type_now());
    // ---- sound: chip (96 kHz) -> resampler -> ring buffer -> the sound card's callback
    Wsg wsg(roms.sound1.data());
    static AudioRing ring(1 << 15); // 32,768 samples, about 0.68 s at 48 kHz: room for stalls
    static AudioStats astats;
    struct AudioCtx {
        std::atomic<bool> playing{false};
        std::vector<int16_t> capture;
        size_t capture_max = 0;
    } actx;
    SDL_AudioDeviceID adev = 0;
    int audio_rate = 48000, audio_buffer = 0;
    auto audio_cb = [](void *ud, Uint8 *stream, int len) {
        AudioCtx *c = (AudioCtx *)ud;
        int16_t *out = (int16_t *)stream;
        size_t n = (size_t)len / 2;
        astats.callbacks++;
        if (!c->playing.load()) {
            std::memset(stream, 0, len);
            return;
        } // waiting for the first blocks (prebuffer)
        size_t level = ring.size();
        astats.level_sum += level;
        astats.level_count++;
        if (level > astats.level_max) astats.level_max = level;
        if (level < astats.level_min) astats.level_min = level;
        size_t got = ring.read(out, n);
        if (got < n) {
            astats.underruns++;
            astats.missing_samples += n - got;
            std::memset(out + got, 0, (n - got) * 2);
        }
        if (c->capture_max && c->capture.size() + n <= c->capture_max) c->capture.insert(c->capture.end(), out, out + n);
    };
    if (!o.no_audio) {
        SDL_AudioSpec want{}, have{};
        want.freq = 48000;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        // 480 samples = 10 ms at 48 kHz (what Windows' shared-mode sound uses; SDL's test driver sleeps a whole number of ms per buffer, so 480 is exact there)
        want.samples = 480;
        want.userdata = &actx;
        want.callback = (SDL_AudioCallback)(void (*)(void *, Uint8 *, int))audio_cb;
        adev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); // 0: SDL converts if the card cannot do exactly this
        if (adev) {
            audio_rate = have.freq;
            audio_buffer = have.samples;
            say("Audio: opened, " + std::to_string(have.freq) + " Hz mono, device buffer " + std::to_string(have.samples) + " samples (" +
                std::to_string(1000.0 * have.samples / have.freq) + " ms)");
        } else say(std::string("Audio: could not open the sound card (") + SDL_GetError() + "); continuing without sound");
    }
    AreaResampler resampler(Wsg::kRate, audio_rate);
    const size_t level_target = (size_t)(0.012 * audio_rate); // wanted ring level just before each new frame of sound is added: 12 ms
    std::vector<int16_t> chip(Wsg::kSamplesPerFrame), rs;
    rs.reserve(2048);
    double sc_max = 1.0, sc_min = 1.0;
    uint64_t frames_off = 0, frames_off1 = 0; // how far (and for how many frames) the resampler was pushed away from the nominal pitch
    bool sound_catching_up = false;           // set by the main loop while it runs frames back to back
    RingGuard guard;
    uint64_t guard_resyncs = 0, guard_dropped = 0;
    uint64_t audio_frames = 0;
    double level_smooth = (double)level_target;
    if (adev && !o.audio_capture.empty()) actx.capture_max = (size_t)audio_rate * 150, actx.capture.reserve(actx.capture_max);
    // the machine side of the sound: after each frame hand the chip its registers and the sound-enable latch, make 1,584 samples, resample, queue
    auto make_sound = [&]() {
        if (!adev) return;
        game::Output out = game::output_view(m.bus);   // N53: the named sound registers and the sound-enable latch
        wsg.load_registers((const uint8_t *)&out.sound);
        wsg.set_enabled(out.latches.sound_enable & 1);
        wsg.render(chip.data(), chip.size());
        // S32: after a freeze (pad unplugged...) keep the queue at its normal level so the pitch controller below never has a big error to correct
        GuardDecision gd = guard.decide(ring.size(), astats.underruns.load(), level_target, sound_catching_up, (size_t)(0.150 * audio_rate));
        if (gd.pad_zeros) {
            static std::vector<int16_t> zeros;
            zeros.assign(gd.pad_zeros, 0);
            ring.write(zeros.data(), zeros.size());
        }
        if (gd.resync) {
            level_smooth = (double)ring.size();
            ++guard_resyncs;
        }
        if (!gd.write) {
            ++guard_dropped;
            ++audio_frames;
            return;
        }
        level_smooth += 0.02 * ((double)ring.size() - level_smooth); // about 0.8 s memory: removes the per-frame sawtooth and jitter
        double sc_now = ring_level_scale((size_t)level_smooth, level_target);
        resampler.set_scale(sc_now);
        if (sc_now > sc_max) { sc_max = sc_now; }
        if (sc_now < sc_min) { sc_min = sc_now; }
        if (std::fabs(sc_now - 1.0) > 0.005) {
            ++frames_off;
            if (std::fabs(sc_now - 1.0) > 0.01) ++frames_off1;
        }
        rs.clear();
        resampler.process(chip.data(), chip.size(), rs);
        size_t put = ring.write(rs.data(), rs.size());
        astats.dropped_samples += rs.size() - put;
        ++audio_frames;
        if (!actx.playing.load() && ring.size() >= 2 * (size_t)(audio_rate / TICK_HZ)) {
            actx.playing.store(true);
            SDL_PauseAudioDevice(adev, 0);
        }
    };

    // ---- S32: the player's controls (never used by the scripted runs and never able to change an input bit: it only picks which controls are pressed)
    controls::Bindings bind;
    {
        std::string problems;
        bool had = controls::load(cfg_path, bind, &problems);
        say(had ? "Controls: loaded " + cfg_path
                : "Controls: no settings file yet (" + (cfg_path.empty() ? std::string("no settings folder") : cfg_path) + "), using the standard controls");
        if (!problems.empty()) say("Controls: some lines of the settings file were ignored:\n" + problems);
    }
    controls::StartAssist start_assist;
    controls::BindEditor bed;
    bool bed_from_hotkey = false;
    controls::TriggerButtons
        trig; // trig: L2/R2 (LT/RT) are axes in SDL; they are turned into button events 100/101   // S34A: the bindings screen replaces the old one-by-one dialogue
    auto save_controls = [&]() {
        std::string err;
        if (controls::save(cfg_path, bind, &err)) say("Controls: saved to " + cfg_path);
        else say("Controls: could not save (" + err + ")");
    };
    auto set_title = [&](const std::string &t) { SDL_SetWindowTitle(win, t.c_str()); };
    std::function<std::string()> game_title; // X03: "Pac-Man", or "Pac-Man - ASSISTED: <reason>" while the running game does not count (set up once the keeper exists)
    Inputs in;
    in.set_dip(applied_dip);
    uint64_t script_tick = 0;
    uint32_t srng = 12345;
    int shold = 0;
    Control sdir = Control::P1Left;
    const int DEAD = 12000;
    auto scripted_inputs = [&]() { // the same script as --autoplay
        const int f = (int)script_tick++;
        const Control dirs[4] = {Control::P1Up, Control::P1Left, Control::P1Right, Control::P1Down};
        in.set(Control::Coin1, f >= 400 && f < 405);
        in.set(Control::Start1, f >= 420 && f < 424);
        for (Control c : dirs) in.set(c, false);
        if (f >= 430) {
            if (shold <= 0) {
                srng = srng * 1664525u + 1013904223u;
                sdir = dirs[(srng >> 24) & 3];
                shold = 10 + (int)((srng >> 16) & 31);
            }
            --shold;
            in.set(sdir, true);
        }
        in.apply(m.bus);
    };
    auto sample_inputs = [&]() {
        if (o.scripted) {
            scripted_inputs();
            return;
        }
        if (replay_on) {
            uint8_t a, b;
            rp.inputs_at(replay_frame++, a, b);
            m.bus.in0 = a;
            m.bus.in1 = b;
            m.bus.dsw1 = rp.dip;
            return;
        } // S34: the recorded inputs, exactly as tools/replay applies them
        if (bed.active()) {
            in.release_all();
            in.apply(m.bus);
            return;
        } // the controls dialogue is open: nothing reaches the game
        const Uint8 *k = SDL_GetKeyboardState(nullptr);
        auto held = [&](controls::Action a) { return (bind.key[a][0] && k[bind.key[a][0]]) || (bind.key[a][1] && k[bind.key[a][1]]); };
        bool up = held(controls::Up), down = held(controls::Down), left = held(controls::Left), right = held(controls::Right);
        bool coin = held(controls::Coin), start1 = held(controls::Start1), start2 = held(controls::Start2);
        int ax = 0, ay = 0;
        if (pad) {
            up |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            down |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            left |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            right |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
            ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
            auto pb = [&](controls::PadAction a) {
                int b = bind.pad[a];
                if (b == controls::kPadLeftTrigger) return trig.left_down();
                if (b == controls::kPadRightTrigger) return trig.right_down();
                return b >= 0 && SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b) != 0;
            };
            coin |= pb(controls::PadCoin);
            start1 |= pb(controls::PadStart1);
            start2 |= pb(controls::PadStart2);
        } else if (joy) {
            if (SDL_JoystickNumHats(joy) > 0) {
                Uint8 h = SDL_JoystickGetHat(joy, 0);
                up |= (h & SDL_HAT_UP) != 0;
                down |= (h & SDL_HAT_DOWN) != 0;
                left |= (h & SDL_HAT_LEFT) != 0;
                right |= (h & SDL_HAT_RIGHT) != 0;
            }
            if (SDL_JoystickNumAxes(joy) >= 2) {
                ax = SDL_JoystickGetAxis(joy, 0);
                ay = SDL_JoystickGetAxis(joy, 1);
            }
        }
        // The stick gives one direction at a time (the stronger axis), like the arcade's four-way joystick; diagonals would press two switches.
        if (std::abs(ax) > DEAD || std::abs(ay) > DEAD) {
            if (std::abs(ax) >= std::abs(ay)) {
                if (ax < 0) left = true;
                else right = true;
            } else {
                if (ay < 0) up = true;
                else down = true;
            }
        }
        in.set(Control::P1Up, up);
        in.set(Control::P1Down, down);
        in.set(Control::P1Left, left);
        in.set(Control::P1Right, right);
        bool ac = false, as = false;
        if (bind.one_button_start && !purist) start_assist.update(start1, m.bus.read(0x4E6E), m.bus.read(0x4E00), &ac, &as); // only reads memory
        in.set(Control::Coin1, coin || ac);
        in.set(Control::Start1, start1 || as);
        in.set(Control::Start2, start2);
        in.apply(m.bus);
    };

    // Fixed timestep: the machine advances exactly one frame per 1/60.606 s of real time, however fast or slow the screen refreshes ("exact" pacing).
    // "smooth" pacing instead ticks once every N refreshes (display::refreshes_per_tick) when that is within 3 % of the arcade rate.
    const uint64_t pf = SDL_GetPerformanceFrequency();
    const double period = (double)pf / TICK_HZ;
    uint64_t start = SDL_GetPerformanceCounter();
    double next = (double)start + period;
    uint64_t ticks = 0, shown = 0;
    bool running = true;
    int steps_this_pass = 0;
    bool pad_rewind_down = false;
    // ---- S33: pause, quick save/load, rewind, high score (none of it runs in the scripted tests, so their hashes are the plain machine's)
    const fsx::path state_dir = cfg_path.empty() ? fsx::path(".") : fsx::path(cfg_path).parent_path();
    const std::string hs_path = o.no_hiscore ? std::string() : (o.hiscore.empty() ? (state_dir / "highscore.txt").string() : o.hiscore);
    state::HighScoreKeeper keeper(hs_path);
    game_title = [&]() { return keeper.assisted() ? std::string("PacArcadeComp - ASSISTED: ") + keeper.rule().label() : std::string("PacArcadeComp"); };
    uint32_t last_assist_mask = 0;
    bool pointer_shown = true;
    lastdevice::Tracker lastdev; // X07: keyboard, mouse or pad? (for the hints that switch pictures, X08); off in Purist mode
    auto lastdev_say = [&]() {
        std::string d = lastdevice::name(lastdev.device());
        if (lastdev.device() == lastdevice::Device::Pad && lastdev.pad_type() != 0) d += std::string(" (") + menu::pad_type_word((menu::PadType)lastdev.pad_type()) + ")";
        say("Input: the last device used is now " + d);
    };
    bool lightbar_pref = st.lightbar;
    lightbar::Driver lbd;
    SDL_GameController *lb_pad = nullptr;
    bool lb_led = false;
    long lb_sends = 0;         // X09: the pad's light bar
    int hints_pref = st.hints; // X08: 0 auto, 1 keyboard, 2 pad
    bool pause_focus_pref = st.pause_focus, focus_paused = false;
    Uint32 focus_gained_ms = 0; // X06: pause when the window loses focus; the game stays paused until a key is pressed
    state::RewindBuffer rewind(60 * 30);
    std::vector<uint8_t> scratch_state;
    long rewound_frames = 0;
    bool paused = false, rewinding = false, force_redraw = false;
    Uint32 flash_until = 0;
    bool extras = !o.scripted && !purist; // S33 extras (save states, rewind, saved high score): off in scripted runs and in Purist mode
    if (extras && !hs_path.empty())
        say(keeper.stored() > 0 ? "High score: " + std::to_string(keeper.stored()) + " in " + hs_path : "High score: no saved score yet (" + hs_path + ")");
    auto flash = [&](const std::string &t) {
        say("State: " + t);
        if (!bed.active()) {
            set_title("PacArcadeComp - " + t);
            flash_until = SDL_GetTicks() + 2500;
        }
    };
    // ---- S34R: vibration. The engine only reads RAM after each frame; the pad's motors are driven here.
    haptics::Engine hx;
    int vib_pref = o.vibration >= 0 ? o.vibration : st.vibration;
    hx.set_level(purist ? 0 : vib_pref);
    haptics::Kind pad_kind = haptics::Kind::Other;
    bool pad_rumble = false, pad_trig_rumble = false;
    haptics::Out rumble_last;
    int rumble_age = 0;
    long rumble_cmds = 0, rumble_events[haptics::kFxCount] = {0};
    float rumble_peak_low = 0, rumble_peak_high = 0, rumble_peak_trig = 0;
    auto probe_pad_rumble = [&]() {
        pad_kind = haptics::Kind::Other;
        pad_rumble = pad_trig_rumble = false;
        rumble_last = haptics::Out();
        rumble_age = 0;
        hx.invalidate();
        if (!pad) return;
        pad_kind = haptics::kind_of_sdl_type((int)SDL_GameControllerGetType(pad));
        pad_rumble = SDL_GameControllerHasRumble(pad) == SDL_TRUE;
        pad_trig_rumble = SDL_GameControllerHasRumbleTriggers(pad) == SDL_TRUE;
        say(std::string("Vibration: ") + haptics::kind_name(pad_kind) + " pad, " + (pad_rumble ? "motors ok" : "no rumble motors") +
            (pad_trig_rumble ? ", trigger motors ok" : ", no trigger motors") + ", setting " +
            (purist          ? "off (Purist)"
             : vib_pref == 0 ? "off"
             : vib_pref == 1 ? "light"
                             : "full"));
    };
    auto rumble_send = [&](const haptics::Out &out) {
        if (!pad || !pad_rumble) return;
        const bool changed = out != rumble_last, quiet = out.low == 0 && out.high == 0 && out.trig_l == 0;
        if (!changed && (quiet || ++rumble_age < 6)) return;
        rumble_age = 0;
        if (quiet) {
            SDL_GameControllerRumble(pad, 0, 0, 0);
            if (pad_trig_rumble) SDL_GameControllerRumbleTriggers(pad, 0, 0, 0);
        } else {
            SDL_GameControllerRumble(pad, haptics::to_u16(out.low), haptics::to_u16(out.high), 120);
            if (pad_trig_rumble) SDL_GameControllerRumbleTriggers(pad, haptics::to_u16(out.trig_l), haptics::to_u16(out.trig_r), 120);
            ++rumble_cmds;
            rumble_peak_low = std::max(rumble_peak_low, out.low);
            rumble_peak_high = std::max(rumble_peak_high, out.high);
            rumble_peak_trig = std::max(rumble_peak_trig, out.trig_l);
        }
        rumble_last = out;
    };
    auto rumble_stop = [&]() {
        hx.silence();
        haptics::Out z;
        if (rumble_last != z) rumble_send(z);
    };
    probe_pad_rumble();
    std::function<void(bool)> lb_step; // X10 (defined further down, after the menu exists)
    auto rumble_frame = [&]() {        // after a machine frame
        if (!pad || !pad_rumble || hx.level() == 0) return;
        haptics::Snap sn;
        auto r = [&](unsigned a) { return (int)m.bus.read(a); };
        sn.main_state = r(0x4E00);
        sn.level_state = r(0x4E04);
        sn.level = r(0x4E13);
        sn.lives = r(0x4E14);
        sn.credits = r(0x4E6E);
        sn.death_anim = r(0x4DA5);
        sn.powered = r(0x4DA6);
        for (int p = 0; p < 2; ++p)
            for (int i = 2; i >= 0; --i) {
                int b = r(0x4E80 + 4 * p + i);
                sn.score += ((b >> 4) * 10 + (b & 15)) * (i == 2 ? 10000 : i == 1 ? 100 : 1);
            }
        sn.pac_y = r(0x4D08);
        sn.pac_x = r(0x4D09);
        for (int i = 0; i < 4; ++i) {
            sn.ghost_y[i] = r(0x4D00 + 2 * i);
            sn.ghost_x[i] = r(0x4D01 + 2 * i);
            sn.edible[i] = r(0x4DA7 + i);
        }
        unsigned ev = hx.feed(sn);
        for (int f = 0; f < haptics::kFxCount; ++f)
            if (ev & (1u << f)) {
                ++rumble_events[f];
                if (f != haptics::kDot) say(std::string("Vibration: ") + haptics::name(f) + " at machine frame " + std::to_string(ticks));
            }
        rumble_send(hx.current(haptics::profile_of(pad_kind), pad_trig_rumble));
        hx.advance();
    };
    // ---- X22: per-run statistics (stats.h) and the game-over summary page. Reads memory only; never in Purist mode.
    int crt_mask = o.crt_mask >= 0 ? o.crt_mask : st.crt_mask;                // X40: 0 off, 1 light, 2 strong
    int glow_level = o.glow >= 0 ? o.glow : st.glow;                          // X41: 0 off, 1 light, 2 strong
    int surround_mode = o.surround_mode >= 0 ? o.surround_mode : st.surround; // X44: 0 black, 1 cabinet
    int curve_level = o.curve >= 0 ? o.curve : st.curve;                      // X42: 0 off, 1 light, 2 strong (curve and vignette)
    std::vector<crt::MeshVertex> curve_vert;
    std::vector<int> curve_idx;
    int curve_dw = -1, curve_dh = -1, curve_dx = -1, curve_dy = -1, curve_built = -1; // the table, rebuilt only when the size or the setting changes
    SDL_Texture *sur_tex = nullptr;
    int sur_w = 0, sur_h = 0;
    surround::Rect sur_pr;
    surround::Image own_surround;                         // X45: the surround texture (generated cabinet or the player's own picture)
    if (surround_mode == surround::kCabinet && !purist) { // X45: surround.png next to the program replaces the generated cabinet
        const std::string sp = (fsx::path(o.exe_dir) / "surround.png").string();
        std::FILE *sf = std::fopen(sp.c_str(), "rb");
        if (sf) {
            std::vector<unsigned char> sb;
            unsigned char tmp[65536];
            size_t got;
            while ((got = std::fread(tmp, 1, sizeof tmp, sf)) > 0 && sb.size() <= surround::kMaxFileBytes) sb.insert(sb.end(), tmp, tmp + got);
            std::fclose(sf);
            const std::string why = surround::decode_png(sb.data(), sb.size(), &own_surround);
            if (why.empty()) say("Surround: using your picture " + sp + " (" + std::to_string(own_surround.w) + " x " + std::to_string(own_surround.h) + ")");
            else say("Surround: could not use " + sp + ": " + why + "; using the generated cabinet");
        } else say("Surround: no surround.png next to the program, using the generated cabinet");
    }
    SDL_Texture *amb_tex = nullptr;
    std::vector<uint32_t> amb_pix((size_t)surround::kAmbientW * surround::kAmbientH); // X46: the small picture behind the picture
    SDL_Texture *vig_tex = nullptr;
    int vig_w = 0, vig_h = 0, vig_level = 0;
    SDL_Texture *glow_tex = nullptr;
    std::vector<uint32_t> glow_pix((size_t)overlay::kOutW * overlay::kOutH);
    SDL_Texture *crt_tex = nullptr;
    int crt_w = 0, crt_h = 0, crt_level = 0;
    bool summary_pref = st.run_summary;
    stats::Collector stc;
    stats::Summary summary_data;
    Uint32 summary_from = 0, summary_until = 0;
    std::string stats_log;
    auto stats_line = [&](const char *kind, int player, const stats::Run &r, long ram_score, unsigned frame) {
        char b[400];
        std::snprintf(
            b, sizeof b,
            "%s P%d frame %u score=%d ram_score=%ld level=%d dots=%d energizers=%d ghosts=%d chain=%d fruit=%d deaths=%d extra=%d frames=%u seconds=%d counted=%d\n",
            kind, player, frame, r.score, ram_score, r.level, r.dots, r.energizers, r.ghosts, r.chain, r.fruit, r.deaths, r.extra_lives, r.frames, r.seconds(),
            r.counted ? 1 : 0);
        stats_log += b;
    };
    // ---- X24: personal bests per level (records.h, records.cfg next to controls.cfg). Only counted games, never in Purist mode.
    const std::string records_path = cfg_path.empty() ? std::string() : (fsx::path(cfg_path).parent_path() / "records.cfg").string();
    records::Table recs;
    if (!records_path.empty() && recs.load(records_path)) say("Records: loaded " + records_path);
    Uint32 best_until = 0;
    std::string best_l1, best_l2;
    std::string records_log;
    // ---- X25: achievements (achievements.h, achievements.cfg next to controls.cfg): only counted games, player 1, 3 lives; never in Purist mode.
    const std::string ach_path = cfg_path.empty() ? std::string() : (fsx::path(cfg_path).parent_path() / "achievements.cfg").string();
    achievements::Engine ach;
    achievements::Popup popup;
    if (!ach_path.empty()) {
        bool bad = false;
        if (ach.load(ach_path, &bad))
            say(bad ? "Achievements: the file " + ach_path + " was damaged; it was set aside as achievements.cfg.bad and nothing is unlocked"
                    : "Achievements: loaded " + ach_path + " (" + std::to_string(ach.count()) + " unlocked)");
    }
    events::Reader evr; // X01: reads memory after each frame; only runs when --events is given (and never in Purist mode)
    // ---- X12: eight save slots. Slot 1 is the old quick save (F2 / F4 / the pad buttons / quicksave.sav); the menu pickers use all eight.
    std::vector<uint8_t> slot_mem[state::kSlots + 1];
    state::SlotInfo slot_nfo[state::kSlots + 1]; // slot_mem: kept in memory when the file could not be written
    auto refresh_slots = [&]() {
        for (int n = 1; n <= state::kSlots; ++n) {
            if (!state::slot_info(state::slot_path(state_dir.string(), n), &slot_nfo[n])) {
                slot_nfo[n] = state::SlotInfo();
                if (!slot_mem[n].empty()) {
                    std::vector<uint8_t> dummy;
                    state::slot_unpack(slot_mem[n], dummy, &slot_nfo[n]);
                }
            }
        }
    };
    refresh_slots();
    auto slot_save = [&](int n) {
        std::vector<uint8_t> st, file;
        state::capture(m, wsg, st, keeper.assisted() ? 1 : 0);
        state::slot_pack(m, st, (int64_t)std::time(nullptr), file);
        std::string err;
        const bool ok = state::save_file(state::slot_path(state_dir.string(), n), file, &err);
        if (!ok) slot_mem[n] = file;
        else slot_mem[n].clear();
        refresh_slots();
        say("State: slot " + std::to_string(n) + (ok ? " saved" : " kept in memory only (the file could not be written: " + err + ")") + " (" +
            state::slot_line(n, slot_nfo[n]).substr(2) + ", lives " + std::to_string(slot_nfo[n].lives) + ")");
        if (n == 1) flash(ok ? "saved (F4 loads it)" : "kept in memory only, the file could not be written (" + err + ")");
        else flash(std::string("saved to slot ") + std::to_string(n) + (ok ? "" : " (memory only: the file could not be written)"));
    };
    auto slot_load = [&](int n) -> bool {
        std::vector<uint8_t> bytes, st;
        std::string err;
        if (!state::load_file(state::slot_path(state_dir.string(), n), bytes, &err)) {
            if (slot_mem[n].empty()) {
                say("State: slot " + std::to_string(n) + " is empty");
                flash(n == 1 ? "nothing saved yet (F2 saves)" : "slot " + std::to_string(n) + " is empty");
                return false;
            }
            bytes = slot_mem[n];
        }
        if (!state::slot_unpack(bytes, st, nullptr)) {
            flash("slot " + std::to_string(n) + " is empty");
            return false;
        }
        uint8_t tag = 1;
        const uint32_t score_before = state::score_ram(m.bus, m.bus.read(0x4E09) ? 1 : 0);
        const int lives_before = m.bus.read(0x4E14);
        if (!state::restore(m, wsg, st, &err, &tag)) {
            flash("could not load: " + err);
            return false;
        }
        keeper.note_state_loaded(m, tag, state::high_score_ram(m.bus));
        rewind.clear();
        force_redraw = true;
        hx.invalidate();
        evr.invalidate();
        stc.invalidate();
        ach.resume_after_load(m.bus.read(0x4E00) == 3 && m.bus.read(0x4E09) == 0 && ((m.bus.dsw1 >> 2) & 3) == 2, m.bus.read(0x4E13) + 1);
        say(std::string("Achievements: after the load the game ") +
            (ach.eligible() ? "still counts for achievements" : "does not count for achievements (not a running one-player game with 3 lives)"));
        lbd.discontinuity();
        rumble_stop();
        {
            state::SlotInfo now;
            std::vector<uint8_t> dummy;
            state::slot_unpack(bytes, dummy, &now);
            say("State: slot " + std::to_string(n) + " loaded (" + state::slot_line(n, now).substr(2) + "; score before the load " + std::to_string(score_before) +
                ", in the machine now " + std::to_string(state::score_ram(m.bus, m.bus.read(0x4E09) ? 1 : 0)) + "; lives before the load " +
                std::to_string(lives_before) + ", in the machine now " + std::to_string(m.bus.read(0x4E14)) + ")");
        }
        flash("loaded (the high score is now the one this state had; what you score from now on in this game will not count for records, but achievements still count)");
        return true;
    };
    auto quick_save = [&]() { slot_save(1); };
    auto quick_load = [&]() { slot_load(1); };
    bool cheat_invincible = false;   // the INVINCIBLE test cheat (Esc > CHEATS > INVINCIBLE): session only, never saved; off in Purist mode and in scripted runs
    auto one_tick = [&]() { // one machine frame, with the S33 extras around it
        if (extras) {
            state::capture(m, wsg, scratch_state);
            rewind.push(scratch_state);
        }
        {   // the INVINCIBLE test cheat: the ghost-touch code ignores ghosts that are not blue (ROM bytes for the original rules, the native copy for the readable rules)
            const bool inv = cheat_invincible && extras;
            cheat::set_invincible(m.bus, inv);
            game::att::g_invincible = inv;
            if (inv && m.bus.read(0x4E00) == 3) keeper.note_assist(assist::kCheatHelp);   // a game that runs with the cheat on does not count
        }
        if (dip_chosen.to_byte() != applied_dip.to_byte() && dipnow::quiet(m)) {   // S34D: lives, bonus life, difficulty or ghost names changed: at the attract screen they are put into the running game (no restart)
            applied_dip = dip_chosen;
            in.set_dip(applied_dip);
            char b[8];
            std::snprintf(b, sizeof b, "%02X", applied_dip.to_byte());
            say(dipnow::apply(m, applied_dip.to_byte()) ? std::string("Game options: DIP settings put into the running game at the attract screen, DIP switches now ") + b + " (no restart)"
                                                       : std::string("Game options: DIP settings could not be put into the running game (restart to apply)"));
        }
        {   // S34E: the log says once when the game settings leave or return to their standard values
            const bool std_now = game_settings_standard();
            if (std_now != ach_settings_said) {
                ach_settings_said = std_now;
                say(std_now ? "Achievements: every game setting is at its standard value again (counts from the next game)" : "Achievements: a game setting is not at its standard value, so no achievement can unlock until it is put back (a game played like that never counts afterwards)");
            }
        }
        sample_inputs();
        m.run_frame();
        make_sound();
        ++ticks;
        ++steps_this_pass;
        rumble_frame();
        lb_step(false);
        if (!o.events_file.empty() && !purist) evr.feed(events::read_raw([&](unsigned a) { return (int)m.bus.read(a); }), (unsigned)ticks);
        if (extras) {
            keeper.after_tick(m);
            if (!purist && rules::user_tuning().active) keeper.note_assist(assist::kModded); // T02i: a modded game does not count
            std::string msg = keeper.take_message();
            if (!msg.empty()) say(msg);
        }
        if (!purist) { // X22 and X24
            const long s1 = state::score_ram(m.bus, 0), s2 = state::score_ram(m.bus, 1);
            if (stc.feed(events::read_raw([&](unsigned a) { return (int)m.bus.read(a); }), (unsigned)ticks, !keeper.assisted(), (int)s1, (int)s2)) {
                summary_data = stc.take();
                stats_line("GAMEOVER", summary_data.player, summary_data.run, summary_data.player == 1 ? s1 : s2, (unsigned)ticks);
                if (summary_pref) {
                    summary_from = SDL_GetTicks();
                    summary_until = summary_from + 7000;
                    force_redraw = true;
                    say(std::string("Stats: game over, summary page shown (") + (summary_data.run.counted ? "counted" : "assisted game, not counted") + ")");
                }
            }
            std::vector<stats::Clear> cl = stc.take_clears();
            if (!o.test_clears.empty() && ticks >= 400 && (ticks - 400) % 120 == 0 && (ticks - 400) / 120 < 64) { // X24 test aid
                int k = 0;
                size_t pos = 0;
                const std::string &tc = o.test_clears;
                while (pos <= tc.size()) {
                    size_t e = tc.find(';', pos);
                    if (e == std::string::npos) e = tc.size();
                    int lv = 0, sc = 0;
                    unsigned fr = 0;
                    if (k == (int)((ticks - 400) / 120) && std::sscanf(tc.substr(pos, e - pos).c_str(), "%d,%d,%u", &lv, &sc, &fr) == 3) {
                        stats::Clear c;
                        c.level = lv;
                        c.score = sc;
                        c.frames = fr;
                        cl.push_back(c);
                    }
                    ++k;
                    pos = e + 1;
                }
            }
            if (ach.feed(stc.log(), achievements::counts_for_achievements(keeper.rule().mask()) && game_settings_standard())) { // X25 + S34E
                for (int id : ach.newly()) {
                    popup.push(id);
                    say(std::string("Achievement unlocked: ") + achievements::def(id).name + " at machine frame " + std::to_string(ticks));
                }
                if (ach.dirty() && !ach_path.empty() && !ach.save(ach_path)) say("Achievements: could not save " + ach_path);
            }
            for (const stats::Clear &c : cl) { // X24: a level was cleared in a game that still counts
                const records::Result rr = recs.update(c.level, c.score, c.frames);
                char b[200];
                std::snprintf(b, sizeof b, "Records: level %d cleared, score %d, time %s (%u frames): %s", c.level, c.score, records::time_text(c.frames).c_str(),
                              c.frames,
                              rr.first                      ? "first record"
                              : rr.new_score && rr.new_time ? "NEW BEST score and time"
                              : rr.new_score                ? "NEW BEST score"
                              : rr.new_time                 ? "NEW BEST time"
                                                            : "no record");
                say(b);
                records_log += std::string(b) + "\n";
                if (rr.first || rr.new_score || rr.new_time) {
                    if (!records_path.empty() && !recs.save(records_path)) say("Records: could not save " + records_path);
                    best_l1 = rr.first ? "RECORD SET" : "NEW BEST";
                    const std::string lv = rr.level == records::kLevels ? "LEVEL 21+" : "LEVEL " + std::to_string(rr.level);
                    best_l2 = rr.first ? lv : lv + (rr.new_score && rr.new_time ? " SCORE+TIME" : rr.new_score ? " SCORE" : " TIME");
                    best_until = SDL_GetTicks() + 3500;
                    force_redraw = true;
                }
            }
        }
        if (hashlog) std::fprintf(hashlog, "%llu %s\n", (unsigned long long)ticks, state_hash(m.bus).c_str()); // S34: "frame sha1" as in tools/replay
        if (replay_on && ticks >= rp.frames) running = false;
    };
    auto rewind_tick = [&]() { // one frame backwards: the machine is put back to the state before its last frame (no sound while rewinding)
        std::vector<uint8_t> s2;
        if (!rewind.pop(s2) || !state::restore(m, wsg, s2, nullptr)) return;
        keeper.note_assist();
        keeper.note_state_replaced(m);
        stc.invalidate();
        ach.invalidate_game();
        ++rewound_frames;
        ++ticks;
        ++steps_this_pass;
    };
    // display state (none of it can reach the emulation)
    display::Fit fit = o.fit ? display::Fit::Fill : display::Fit::Integer;
    bool scanlines = o.scanlines, overlay_on = o.overlay_on, bordered = !o.borderless, smooth = false, smooth_pref = o.smooth;
    int smooth_n = 0;
    double smooth_hz = 0;
    double smooth_next = 0;
    int smooth_since = 0; // smooth_next: the time of the next tick on the clock; smooth_since: refreshes since the last tick
    auto set_pacing = [&](bool want_smooth) {
        smooth = false;
        smooth_n = 0;
        if (want_smooth) {
            smooth_n = display::refreshes_per_tick(refresh_hz, TICK_HZ, 0.03, &smooth_hz);
            if (smooth_n > 0 && (vsync || o.sim_refresh > 0)) {
                smooth = true;
                char b[300];
                std::snprintf(b, sizeof b, "Pacing: smooth: %d refreshes per tick = %.3f ticks per second (arcade %.3f, %.2f %% %s)", smooth_n, smooth_hz, TICK_HZ,
                              std::fabs(smooth_hz - TICK_HZ) / TICK_HZ * 100, smooth_hz < TICK_HZ ? "slow" : "fast");
                say(b);
            } else
                say("Pacing: smooth is not possible here (" +
                    std::string(refresh_hz <= 0                  ? "refresh rate unknown"
                                : (!vsync && o.sim_refresh <= 0) ? "no vsync"
                                                                 : "no whole number of refreshes is within 3 % of 60.606 Hz") +
                    "); staying with exact");
        }
        if (!smooth) say("Pacing: exact (60.606 ticks per second)");
        next = (double)SDL_GetPerformanceCounter() + period;
        smooth_next = (double)SDL_GetPerformanceCounter();
        smooth_since = smooth_n;
    };
    set_pacing(smooth_pref && !purist);
    hx.set_level(purist ? 0 : vib_pref);
    rumble_stop();
    say(std::string("Surround: ") + (purist ? "black (Purist)" : surround::name(surround_mode)));
    say(std::string("Curve: ") + (purist ? "off (Purist)" : crt::name(curve_level)));
    say(std::string("Glow: ") + (purist ? "off (Purist)" : crt::name(glow_level)));
    say(std::string("CRT mask: ") + (purist ? "off (Purist)" : crt::name(crt_mask)));
    say(std::string("Display: ") + (fit == display::Fit::Fill ? "fill" : "whole-number") + " scaling, scanlines " + (scanlines ? "on" : "off") + ", window frame " +
        (bordered ? "on" : "off") + ", overlay " + (overlay_on ? "on" : "off"));
    static uint32_t over[overlay::kOutW * overlay::kOutH];
    display::PacingStats pst;
    uint64_t last_present = SDL_GetPerformanceCounter(), presents = 0;
    double sim_next = (double)SDL_GetPerformanceCounter();
    long burst_tick = -1, burst_ms = 0;
    double burst_end = 0;
    bool burst_over = false; // S36 test aid (--burst)
    {
        size_t c = o.burst.find(':');
        if (c != std::string::npos) {
            burst_tick = std::atol(o.burst.c_str());
            burst_ms = std::atol(o.burst.c_str() + c + 1);
        }
    }
    struct KeyAt {
        long tick;
        SDL_Keycode key;
        bool done;
    };
    std::vector<KeyAt> keyat;
    {
        std::string k = o.keys;
        size_t p0 = 0;
        while (p0 < k.size()) {
            size_t p1 = k.find(',', p0);
            if (p1 == std::string::npos) p1 = k.size();
            std::string it = k.substr(p0, p1 - p0);
            p0 = p1 + 1;
            size_t at = it.find('@');
            if (at == std::string::npos) continue;
            std::string kn = it.substr(0, at);
            SDL_Keycode kc = kn == "F3"    ? SDLK_F3
                             : kn == "F5"  ? SDLK_F5
                             : kn == "F6"  ? SDLK_F6
                             : kn == "F7"  ? SDLK_F7
                             : kn == "F8"  ? SDLK_F8
                             : kn == "F11" ? SDLK_F11
                                           : SDLK_UNKNOWN;
            if (kc != SDLK_UNKNOWN) keyat.push_back({std::atol(it.c_str() + at + 1), kc, false});
        }
    }
    struct CmdAt {
        long tick;
        menu::Cmd cmd;
        bool done;
    };
    std::vector<CmdAt> cmdat;
    {   // S34D test aid --cmd-at
        std::string k = o.cmd_at;
        size_t p0 = 0;
        while (p0 < k.size()) {
            size_t p1 = k.find(',', p0);
            if (p1 == std::string::npos) p1 = k.size();
            const std::string it = k.substr(p0, p1 - p0);
            p0 = p1 + 1;
            const size_t c = it.find(':');
            if (c == std::string::npos) continue;
            const std::string n = it.substr(c + 1);
            using menu::Cmd;
            const Cmd cm = n == "resetdisplay" ? Cmd::ResetDisplay : n == "resetgame" ? Cmd::ResetGame : n == "lives" ? Cmd::Lives : n == "bonus" ? Cmd::Bonus : n == "difficulty" ? Cmd::Difficulty : n == "names" ? Cmd::Names
                         : n == "overlay" ? Cmd::Overlay : n == "scanlines" ? Cmd::Scanlines : n == "rules" ? Cmd::Rules : n == "surround" ? Cmd::Surround : n == "restart" ? Cmd::Restart : Cmd::None;
            if (cm != Cmd::None) cmdat.push_back({std::atol(it.c_str()), cm, false});
        }
    }
    struct PadAt {
        long tick;
        int button;
        bool down;
        bool done;
    };
    std::vector<PadAt> padat;
    const Uint32 t_start_ms = SDL_GetTicks(); // test aid: pushes real SDL pad-button events MS milliseconds after the program started (the game may be paused by then)
    {
        std::string k = o.pad_buttons;
        size_t p0 = 0;
        while (p0 < k.size()) {
            size_t p1 = k.find(',', p0);
            if (p1 == std::string::npos) p1 = k.size();
            std::string it = k.substr(p0, p1 - p0);
            p0 = p1 + 1;
            size_t c = it.find(':');
            if (c == std::string::npos) continue;
            std::string bn = it.substr(c + 1);
            bool dn = true;
            if (!bn.empty() && bn[0] == '-') {
                dn = false;
                bn = bn.substr(1);
            }
            bool ok = false;
            int bt = controls::pad_from_name(bn, &ok);
            if (ok && bt >= 0) padat.push_back({std::atol(it.c_str()), bt, dn, false});
        }
    }
    struct VAt {
        long ms;
        bool axis;
        int index;
        int value;
        bool done;
    };
    std::vector<VAt> vat; // virtual pad changes at wall-clock milliseconds
    {
        std::string k = o.pad_virtual;
        size_t p0 = 0;
        while (p0 < k.size()) {
            size_t p1 = k.find(',', p0);
            if (p1 == std::string::npos) p1 = k.size();
            std::string it = k.substr(p0, p1 - p0);
            p0 = p1 + 1;
            size_t c = it.find(':'), q = it.find('=');
            if (c == std::string::npos || q == std::string::npos || q < c + 2) continue;
            char kind = it[c + 1];
            if (kind != 'a' && kind != 'b') continue;
            vat.push_back({std::atol(it.c_str()), kind == 'a', std::atoi(it.c_str() + c + 2), std::atoi(it.c_str() + q + 1), false});
        }
    }
    struct Stall {
        long tick;
        long ms;
        bool done;
    };
    std::vector<Stall> stalls;
    {
        std::string k = o.stall;
        size_t p0 = 0;
        while (p0 < k.size()) {
            size_t p1 = k.find(',', p0);
            if (p1 == std::string::npos) p1 = k.size();
            std::string it = k.substr(p0, p1 - p0);
            p0 = p1 + 1;
            size_t c = it.find(':');
            if (c != std::string::npos) stalls.push_back({std::atol(it.c_str()), std::atol(it.c_str() + c + 1), false});
        }
    }
    auto resize_for_content = [&](bool was_panel,
                                  bool now_panel) { // a side panel (overlay or key help) grows or shrinks the window by the panel, keeping the picture size
        if (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED)) return;
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        int s = std::max(1, std::min(ww / (was_panel ? overlay::kOutW : W), wh / H));
        SDL_SetWindowSize(win, (now_panel ? overlay::kOutW : W) * s, H * s);
    };
    // ---- S34: the menu. Every function key and every menu item goes through do_cmd, so they always do the same thing.
    menu::Menu mn;
    menu::Model model;
    int reset_armed = 0;   // S34E: 0 nothing asked, 1 DISPLAY reset asked, 2 GAME OPTIONS reset asked (the first Enter on a RESET TO DEFAULTS item arms it, the second one does it (opening or closing the menu disarms it)
    auto fill_model = [&]() {
        model.purist = purist;
        model.overlay = overlay_on;
        model.scanlines = scanlines;
        model.smooth = smooth_pref;
        model.frame = bordered;
        model.fill = fit == display::Fit::Fill;
        model.fullscreen = (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
        model.one_button = bind.one_button_start;
        model.help_pin = help_pin;
        model.vibration = vib_pref;
        model.pause_focus = pause_focus_pref;
        model.pad_symbols = pad_symbols;
        if (mn.is_open() && mn.page() >= 4) refresh_slots(); // X12: the picker shows what the files say now
        for (int i = 0; i < state::kSlots; ++i) model.slot_line[i] = state::slot_line(i + 1, slot_nfo[i + 1]);
        for (int i = 0; i < records::kLevels; ++i) { model.rec_line[i] = records::line(i + 1, recs.get(i + 1)); }
        model.hints = hints_pref;
        model.lightbar = lightbar_pref;
        model.run_summary = summary_pref;
        model.crt_mask = crt_mask;
        model.glow = glow_level;
        model.curve = curve_level;
        model.surround = surround_mode;
        model.ach_mask = ach.mask();
        model.ach_blocked = !game_settings_standard();
        model.cheat_invincible = cheat_invincible;
        for (int i = 0; i < achievements::kCount && i < 24; ++i) ach.progress(i, &model.ach_have[i], &model.ach_need[i]);
        model.hint_device = (int)lastdev.hint_device();
        for (int i = 0; i < 3; ++i) model.pad_game_btn[i] = bind.pad[i];
        menu::binding_keycaps(bind, model.key_cap);
        model.lives = (int)dip_chosen.lives;
        model.bonus = (int)dip_chosen.bonus;
        model.diff_normal = dip_chosen.normal_difficulty;
        model.names_normal = dip_chosen.normal_ghost_names;
        model.rules_readable = rules_pref;
        model.rules_file = rules::user_tuning().active;
        model.assist = keeper.assisted() ? keeper.rule().label() : std::string();
        model.pending = dip_chosen.to_byte() != applied_dip.to_byte();
        menu::binding_lines(bind, model.bind_line);
        menu::pad_names(bind, model.pad_name);
        menu::pad_buttons(bind, model.pad_btn);
        model.pad_type = pad_type_now();
    };
    auto panel_on = [&]() { return mn.is_open() || (overlay_on && !purist) || help_pin; };
    bool panel_was = panel0;
    auto persist = [&]() {
        if (!use_settings) return;
        menu::Settings x;
        x.overlay = overlay_on;
        x.scanlines = scanlines;
        x.fill = fit == display::Fit::Fill;
        x.smooth = smooth_pref;
        x.frame = bordered;
        x.help_pin = help_pin;
        x.vibration = vib_pref;
        x.pause_focus = pause_focus_pref;
        x.hints = hints_pref;
        x.lightbar = lightbar_pref;
        x.run_summary = summary_pref;
        x.crt_mask = crt_mask;
        x.glow = glow_level;
        x.curve = curve_level;
        x.surround = surround_mode;
        x.pad_symbols = pad_symbols;
        x.lives = (int)dip_chosen.lives;
        x.bonus = (int)dip_chosen.bonus;
        x.diff_normal = dip_chosen.normal_difficulty;
        x.names_normal = dip_chosen.normal_ghost_names;
        x.rules_readable = rules_pref;
        std::string e;
        if (!menu::save_settings(settings_path, x, &e)) say("Settings: could not save (" + e + ")");
    };
    auto restart_machine =
        [&](const std::string &why) { // power the machine off and on again: the DIP switches are read now, the high score is put in again at the first attract screen
            if (extras) {
                keeper.finish(m);
                std::string msg = keeper.take_message();
                if (!msg.empty()) say(msg);
            }
            m.~Machine();
            new (&m) Machine(roms.program);
            m.rules = rules_table(); // T02a, T02i
            wsg.~Wsg();
            new (&wsg) Wsg(roms.sound1.data());
            keeper.~HighScoreKeeper();
            new (&keeper) state::HighScoreKeeper(hs_path);
            start_assist = controls::StartAssist();
            rewind.clear();
            applied_dip = dip_chosen;
            in.set_dip(applied_dip);
            paused = false;
            rewinding = false;
            force_redraw = true;
            script_tick = 0;
            hx.invalidate(true);
            evr.invalidate(true);
            stc.invalidate(true);
            ach.invalidate_game();
            popup.clear();
            summary_until = 0;
            lbd.discontinuity(true);
            rumble_stop();
            say("Game: restarted (" + why + "), DIP switches now " + [&] {
                char b[8];
                std::snprintf(b, sizeof b, "%02X", applied_dip.to_byte());
                return std::string(b);
            }());
        };
    auto open_menu = [&]() {
        reset_armed = 0;
        fill_model();
        mn.note.clear();
        mn.open();
        paused = true;
        force_redraw = true;
        set_title("PacArcadeComp - MENU");
        say("Menu: opened (the game is paused)");
    };
    auto close_menu = [&]() {
        reset_armed = 0;
        mn.close();
        mn.note.clear();
        paused = false;
        force_redraw = true;
        set_title(game_title());
    };
    lb_step =
        [&](bool
                screen_frame) { // the light bar (outside the machine; only changes are sent): called after every machine frame (Play) and on screen frames while the menu / a pause is up
            if (pad != lb_pad) {
                lbd.invalidate();
                lb_pad = pad;
                lb_led = pad && SDL_GameControllerHasLED(pad) == SDL_TRUE;
            }
            if (!lb_led) return;
            const bool menu_up = paused || mn.is_open() || bed.active() || focus_paused;
            if (!screen_frame && menu_up) return;
            lightbar::Situation sit;
            const bool want_sit = !menu_up && lightbar_pref && !purist;
            if (want_sit) {
                auto r = [&](unsigned a) { return (int)m.bus.read(a); };
                sit.main_state = r(0x4E00);
                sit.level_state = r(0x4E04);
                sit.death_anim = r(0x4DA5);
                sit.powered = r(0x4DA6);
                sit.pac_y = r(0x4D08);
                sit.pac_x = r(0x4D09);
                sit.lives = r(0x4E14);
                sit.dots = r(0x4E0E);
                sit.player = r(0x4E09);
                for (int pl = 0; pl < 2; ++pl)
                    for (int k = 2; k >= 0; --k) {
                        int bb = r(0x4E80 + 4 * pl + k);
                        sit.score += ((bb >> 4) * 10 + (bb & 15)) * (k == 2 ? 10000 : k == 1 ? 100 : 1);
                    }
                for (int i = 0; i < 4; ++i) {
                    sit.ghost_y[i] = r(0x4D00 + 2 * i);
                    sit.ghost_x[i] = r(0x4D01 + 2 * i);
                    sit.edible[i] = r(0x4DA7 + i);
                }
            }
            lightbar::Rgb c;
            const bool send = lbd.update(true, lightbar_pref, purist, menu_up ? lightbar::Scene::Menu : lightbar::Scene::Play, want_sit ? &sit : nullptr, &c);
            if (lbd.flash_started() != lightbar::Flash::None)
                say(std::string("Light bar flash: ") + lightbar::flash_def(lbd.flash_started()).name + " at machine frame " + std::to_string(ticks));
            if (lbd.mood_changed()) {
                lightbar::Rgb t = lbd.target();
                say("Light bar: " + std::to_string(t.r) + "," + std::to_string(t.g) + "," + std::to_string(t.b) + " (" + lightbar::mood_name(lbd.mood()) +
                    ") at machine frame " + std::to_string(ticks));
            }
            if (send) {
                SDL_GameControllerSetLED(pad, c.r, c.g, c.b);
                ++lb_sends;
            }
        };
    auto do_cmd = [&](menu::Cmd c, int delta) {
        using menu::Cmd;
        if (c != Cmd::ResetDisplay && c != Cmd::ResetGame) reset_armed = 0;
        fill_model();
        if (menu::locked(c, model)) {
            mn.note = "LOCKED (TEST FLAG)";
            flash("that is switched off by the --purist test flag");
            return;
        }
        mn.note.clear();
        auto step4 = [&](int v) { return (v + (delta < 0 ? 3 : 1)) & 3; };
        switch (c) {
        case Cmd::Resume:
            close_menu();
            say("Menu: resumed");
            break;
        case Cmd::SaveState:
            if (extras) {
                quick_save();
                mn.note = "STATE SAVED";
            } else say("State: saving is not available here");
            break;
        case Cmd::LoadState:
            if (extras) {
                quick_load();
                if (mn.is_open()) close_menu();
            } else say("State: loading is not available here");
            break;
        case Cmd::SaveSlot:
            if (extras && delta >= 1 && delta <= state::kSlots) {
                slot_save(delta);
                mn.note = "SAVED TO SLOT " + std::to_string(delta);
            } else say("State: saving is not available here");
            break;
        case Cmd::LoadSlot:
            if (extras && delta >= 1 && delta <= state::kSlots) {
                if (slot_load(delta)) close_menu();
                else mn.note = "SLOT " + std::to_string(delta) + " IS EMPTY";
            } else say("State: loading is not available here");
            break;
        case Cmd::Lives:
            dip_chosen.lives = (DipSettings::Lives)step4((int)dip_chosen.lives);
            persist();
            say("Game options: lives " + menu::value_text(c, (fill_model(), model)) + " (applies from the next game, no restart needed)");
            break;
        case Cmd::Bonus:
            dip_chosen.bonus = (DipSettings::Bonus)step4((int)dip_chosen.bonus);
            persist();
            fill_model();
            say("Game options: bonus life " + menu::value_text(c, model) + " (applies from the next game, no restart needed)");
            break;
        case Cmd::Difficulty:
            dip_chosen.normal_difficulty = !dip_chosen.normal_difficulty;
            persist();
            fill_model();
            say("Game options: difficulty " + menu::value_text(c, model) + " (applies from the next game, no restart needed)");
            break;
        case Cmd::CheatInvincible:
            cheat_invincible = !cheat_invincible;
            fill_model();
            say(std::string("Cheats: invincible ") + (cheat_invincible ? "on (ghosts that are not blue cannot catch Pac-Man; the game stops counting for records and saved achievements)" : "off"));
            break;
        case Cmd::Names:
            dip_chosen.normal_ghost_names = !dip_chosen.normal_ghost_names;
            persist();
            fill_model();
            say("Game options: ghost names " + menu::value_text(c, model) + " (applies from the next game, no restart needed)");
            break;
        case Cmd::Rules:
            if (rules::user_tuning().active) {
                mn.note = "A RULES FILE IS IN USE";
                say("Game options: a rules file is in use, so the game runs the readable rules (remove rules.cfg to choose)");
                break;
            }
            rules_pref = !rules_pref;
            m.rules = rules_table(); // the readable rules give the same frames as the ROM routines, so the switch can be made at any moment
            persist();
            fill_model();
            say(std::string("Game options: rules ") + menu::value_text(c, model));
            break;
        case Cmd::WriteRules: {
            const std::string dir = settings_path.empty() ? std::string() : fsx::path(settings_path).parent_path().string();
            const std::string target = dir.empty() ? std::string("rules.cfg") : (fsx::path(dir) / "rules.cfg").string();
            std::error_code ec;
            std::string e;
            if (fsx::exists(target, ec)) {
                mn.note = "RULES.CFG EXISTS ALREADY";
                say("Rules file: " + target + " exists already, not overwritten");
            } else if (!rules::write_standard_file(target, &e)) {
                mn.note = "COULD NOT WRITE";
                say("Rules file: " + e);
            } else {
                mn.note = "RULES.CFG WRITTEN";
                say("Rules file: wrote the standard rules to " + target + " (edit it with Notepad, then restart the program)");
            }
            break;
        }
        case Cmd::Restart:
            restart_machine("from the menu");
            close_menu();
            break;
        case Cmd::Bindings:
        case Cmd::Keys:
        case Cmd::Pad: { // F9 / F10 / the menu item all open the same screen (F10 starts on the pad column)
            if (c == Cmd::Pad && !pad) { say("Controls: no standard gamepad is connected (the screen opens on the keys)"); }
            bool was_open = mn.is_open();
            if (!was_open) {
                mn.note.clear();
                mn.open();
                paused = true;
            }
            bed_from_hotkey = !was_open;
            bed.begin(bind, c == Cmd::Pad && pad ? (int)controls::Coin : 0, c == Cmd::Pad && pad ? 2 : 0);
            force_redraw = true;
            set_title("PacArcadeComp - BINDINGS");
            say("Controls: bindings screen opened");
            break;
        }
        case Cmd::ResetControls:
            bind = controls::Bindings();
            save_controls();
            say("Controls: back to the standard controls");
            mn.note = "KEYS RESET";
            break;
        case Cmd::OneButton:
            bind.one_button_start = !bind.one_button_start;
            save_controls();
            say(std::string("Controls: one-button Start ") + (bind.one_button_start ? "on" : "off"));
            break;
        case Cmd::Overlay:
            overlay_on = !overlay_on;
            persist();
            say(std::string("Display: overlay ") + (overlay_on ? "on" : "off"));
            break;
        case Cmd::Scanlines:
            scanlines = !scanlines;
            persist();
            say(std::string("Display: scanlines ") + (scanlines ? "on" : "off"));
            break;
        case Cmd::Pacing:
            set_pacing(!smooth);
            smooth_pref = smooth;
            persist();
            break;
        case Cmd::Frame:
            bordered = !bordered;
            SDL_SetWindowBordered(win, bordered ? SDL_TRUE : SDL_FALSE);
            persist();
            say(std::string("Display: window frame ") + (bordered ? "on" : "off"));
            break;
        case Cmd::Scaling:
            fit = fit == display::Fit::Integer ? display::Fit::Fill : display::Fit::Integer;
            persist();
            say(std::string("Display: ") + (fit == display::Fit::Fill ? "fill" : "whole-number") + " scaling");
            break;
        case Cmd::FullScreen: {
            bool fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            say(std::string("Display: full screen ") + (fs ? "off" : "on"));
            break;
        }
        case Cmd::PadSymbols:
            pad_symbols = (pad_symbols + (delta < 0 ? 3 : 1)) % 4;
            persist();
            say(std::string("Controls: pad symbols ") + (pad_symbols == 0 ? "auto" : pad_symbols == 1 ? "Xbox" : pad_symbols == 2 ? "PS4" : "PS5"));
            break;
        case Cmd::Surround:
            surround_mode = surround::next_mode(surround_mode);
            persist();
            say(std::string("Display: surround ") + surround::name(surround_mode));
            break;
        case Cmd::CrtLook: { // X43: one step through the presets (mask + glow + curve + scanlines together)
            const crt::Preset &pr = crt::preset(crt::preset_step(crt_mask, glow_level, curve_level, scanlines, delta));
            crt_mask = pr.mask;
            glow_level = pr.glow;
            curve_level = pr.curve;
            scanlines = pr.scanlines;
            persist();
            say(std::string("Display: CRT look ") + pr.name + " (mask " + crt::name(pr.mask) + ", glow " + crt::name(pr.glow) + ", curve " + crt::name(pr.curve) +
                ", scanlines " + (pr.scanlines ? "on" : "off") + ")");
            break;
        }
        case Cmd::Curve:
            curve_level = (curve_level + (delta < 0 ? 2 : 1)) % 3;
            persist();
            say(std::string("Display: curve ") + crt::name(curve_level) + (curve_level ? " (the curve is flat while this menu is open)" : ""));
            break;
        case Cmd::Glow:
            glow_level = (glow_level + (delta < 0 ? 2 : 1)) % 3;
            persist();
            say(std::string("Display: glow ") + crt::name(glow_level));
            break;
        case Cmd::CrtMask:
            crt_mask = (crt_mask + (delta < 0 ? 2 : 1)) % 3;
            persist();
            say(std::string("Display: CRT mask ") + crt::name(crt_mask));
            break;
        case Cmd::RunSummary:
            summary_pref = !summary_pref;
            if (!summary_pref) summary_until = 0;
            persist();
            say(std::string("Display: run summary page ") + (summary_pref ? "on" : "off"));
            break;
        case Cmd::Lightbar:
            lightbar_pref = !lightbar_pref;
            persist();
            say(std::string("Controls: light bar ") + (lightbar_pref ? "on" : "off"));
            break;
        case Cmd::Hints:
            hints_pref = (hints_pref + 1) % 3;
            persist();
            say(std::string("Controls: button pictures ") + (hints_pref == 1 ? "always keyboard" : hints_pref == 2 ? "always pad" : "follow the last device used"));
            break;
        case Cmd::FocusPause:
            pause_focus_pref = !pause_focus_pref;
            persist();
            say(std::string("Controls: pause when the window loses focus ") + (pause_focus_pref ? "on" : "off"));
            break;
        case Cmd::Vibration:
            vib_pref = (vib_pref + (delta < 0 ? 2 : 1)) % 3;
            persist();
            hx.set_level(purist ? 0 : vib_pref);
            say(std::string("Controls: vibration ") + (vib_pref == 0 ? "off" : vib_pref == 1 ? "light" : "full"));
            if (hx.level() && pad && pad_rumble) {
                hx.preview();
                rumble_send(hx.current(haptics::profile_of(pad_kind), pad_trig_rumble));
                hx.advance();
            }
            break;
        case Cmd::HelpPin:
            help_pin = !help_pin;
            persist();
            say(std::string("Display: key help beside the game ") + (help_pin ? "on" : "off"));
            break;
        case Cmd::ResetDisplay: {   // S34E: DISPLAY > RESET TO DEFAULTS: only the look, at once, the game is not touched
            if (reset_armed != 1) {
                reset_armed = 1;
                mn.note = "ENTER AGAIN TO RESET";
                say("Menu: press Enter again on DISPLAY > RESET TO DEFAULTS to put the display settings back to their standard values (the game is not restarted and not changed)");
                break;
            }
            reset_armed = 0;
            const menu::Settings d;   // the standard settings: the one list in docs/notes/settings_table.md
            overlay_on = d.overlay;
            scanlines = d.scanlines;
            fit = d.fill ? display::Fit::Fill : display::Fit::Integer;
            set_pacing(d.smooth);
            smooth_pref = d.smooth;
            if (bordered != d.frame) {
                bordered = d.frame;
                SDL_SetWindowBordered(win, bordered ? SDL_TRUE : SDL_FALSE);
            }
            help_pin = d.help_pin;
            summary_pref = d.run_summary;
            if (!summary_pref) summary_until = 0;
            crt_mask = d.crt_mask;
            glow_level = d.glow;
            curve_level = d.curve;
            surround_mode = d.surround;
            persist();
            fill_model();
            say("Settings: the display settings are back to their standard values (overlay, scanlines, pacing, scaling, window frame, key panel, run summary, CRT mask, glow, curve, surround); nothing else was changed and the game was not restarted");
            mn.note = "DISPLAY RESET";
            break;
        }
        case Cmd::ResetGame: {   // S34E: GAME OPTIONS > RESET TO DEFAULTS: only the game settings; the game restarts so that they take effect
            if (reset_armed != 2) {
                reset_armed = 2;
                mn.note = "ENTER AGAIN. RESTARTS";
                say("Menu: press Enter again on GAME OPTIONS > RESET TO DEFAULTS to put the game settings back to their standard values; THE GAME RESTARTS (keys, buttons, high score, records, achievements and save slots are kept)");
                break;
            }
            reset_armed = 0;
            const menu::Settings d;
            rules_pref = d.rules_readable;
            dip_chosen.lives = (DipSettings::Lives)d.lives;
            dip_chosen.bonus = (DipSettings::Bonus)d.bonus;
            dip_chosen.normal_difficulty = d.diff_normal;
            dip_chosen.normal_ghost_names = d.names_normal;
            persist();
            say(std::string("Settings: the game settings are back to their standard values (lives, bonus life, difficulty, ghost names, rules); display, sound, keys, buttons, high score, records, achievements and save slots were not changed") +
                (rules::user_tuning().active ? "; a rules file (rules.cfg) is still in use, so the readable rules still run (remove the file to go back to the original rules)" : ""));
            restart_machine("game settings reset");
            close_menu();
            break;
        }
        case Cmd::Locked:
            mn.note = "LOCKED (TEST FLAG)";
            flash("that is switched off by the --purist test flag");
            break;
        case Cmd::Quit:
            running = false;
            say("Menu: quit");
            break;
        default: break;
        }
    };
    auto handle_key = [&](SDL_Keycode key) {
        if (key == SDLK_F11) do_cmd(menu::Cmd::FullScreen, 1);
        else if (key == SDLK_F3) do_cmd(menu::Cmd::Overlay, 1);
        else if (key == SDLK_F5) do_cmd(menu::Cmd::Scanlines, 1);
        else if (key == SDLK_F6) do_cmd(menu::Cmd::Pacing, 1);
        else if (key == SDLK_F7) do_cmd(menu::Cmd::Frame, 1);
        else if (key == SDLK_F8) do_cmd(menu::Cmd::Scaling, 1);
    };
    auto menu_key = [&](menu::Key k) {
        fill_model();
        menu::Result r = mn.key(k, model);
        if (r.cmd != menu::Cmd::None) do_cmd(r.cmd, r.delta);
    };
    if (o.borderless) SDL_SetWindowBordered(win, SDL_FALSE);
    while (running) {
        if (pad) { // S34B fix: the triggers are axes; turn a pull into the same button events as any other button (so binding, the menu and the system commands all see them)
            std::vector<controls::TriggerButtons::Change> ch;
            trig.update(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT), SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), ch);
            for (const auto &c : ch) {
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = c.down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
                ev.cbutton.which = 0;
                ev.cbutton.button = (Uint8)c.button;
                ev.cbutton.state = c.down ? SDL_PRESSED : SDL_RELEASED;
                SDL_PushEvent(&ev);
            }
        }
        {
            bool want_ptr = mn.is_open();
            if (want_ptr != pointer_shown) {
                SDL_ShowCursor(want_ptr ? SDL_ENABLE : SDL_DISABLE);
                pointer_shown = want_ptr;
                say(want_ptr ? "Menu: mouse pointer shown" : "Menu: mouse pointer hidden");
            }
        } // X04: the pointer shows only while the menu is open
        {
            bool want = panel_on();
            if (want != panel_was) {
                resize_for_content(panel_was, want);
                panel_was = want;
                force_redraw = true;
            }
        }
        if (!purist && lastdev.pad_type((int)pad_type_now())) lastdev_say();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!purist) { // X07: only looks at the event
                bool dch = false;
                switch (e.type) {
                case SDL_KEYDOWN: dch = lastdev.key(); break;
                case SDL_MOUSEMOTION: dch = lastdev.mouse_motion(e.motion.xrel, e.motion.yrel); break;
                case SDL_MOUSEBUTTONDOWN: dch = lastdev.mouse_click(); break;
                case SDL_MOUSEWHEEL: dch = lastdev.mouse_wheel(); break;
                case SDL_CONTROLLERBUTTONDOWN: dch = lastdev.pad_button(); break;
                case SDL_CONTROLLERAXISMOTION:
                    if (e.caxis.axis <= SDL_CONTROLLER_AXIS_RIGHTY) dch = lastdev.pad_stick(e.caxis.value);
                    break;
                default: break;
                }
                if (dch) lastdev_say();
            }
            if (summary_until && ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_MOUSEBUTTONDOWN) &&
                SDL_GetTicks() - summary_from > 700) {
                summary_until = 0;
                force_redraw = true;
                say("Stats: summary page closed (a key was pressed)");
            } // X22: the key still goes on to the game
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_MINIMIZED) && pause_focus_pref &&
                     !purist && !o.scripted && !replay_on) {
                if (!paused && !mn.is_open() && !bed.active()) {
                    paused = true;
                    focus_paused = true;
                    force_redraw = true;
                    set_title("PacArcadeComp - PAUSED (PRESS ANY KEY)");
                    say("Focus: the window lost focus, the game is paused (press any key to go on)");
                }
            } else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_RESTORED)) {
                focus_gained_ms = SDL_GetTicks();
                if (focus_paused) force_redraw = true;
            } else if (focus_paused &&
                       (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN || (e.type == SDL_MOUSEBUTTONDOWN && SDL_GetTicks() - focus_gained_ms > 400))) {
                if (e.type != SDL_KEYDOWN || !e.key.repeat) {
                    focus_paused = false;
                    paused = false;
                    force_redraw = true;
                    set_title(game_title());
                    say("Focus: going on (a key was pressed)");
                }
            } else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_JOYDEVICEADDED) {
                list_pads();
                if (!pad && !joy) {
                    openPads();
                    announce_pad("Gamepad connected: ");
                    probe_pad_rumble();
                    if (pad || joy) toast(true, pad_type_now());
                }
            } else if (e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEREMOVED) {
                if (pad && !SDL_GameControllerGetAttached(pad)) {
                    menu::PadType gone = pad_type_now();
                    toast(false, gone);
                    SDL_GameControllerClose(pad);
                    pad = nullptr;
                    padName.clear();
                    announcer.forget();
                    say("Gamepad removed");
                    pad_rewind_down = false;
                    trig.reset();
                    hx.silence();
                    probe_pad_rumble();
                    openPads();
                    announce_pad("Gamepad connected: ");
                    probe_pad_rumble();
                    if (pad || joy) toast(true, pad_type_now());
                }
                if (joy && !SDL_JoystickGetAttached(joy)) {
                    toast(false, menu::PadType::Other);
                    SDL_JoystickClose(joy);
                    joy = nullptr;
                    padName.clear();
                    announcer.forget();
                    say("Gamepad removed");
                    openPads();
                    announce_pad("Gamepad connected: ");
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN &&
                       bed.active()) { // the pad works the bindings screen: d-pad moves, A selects, B back, X clears; while waiting any button is the answer
                int b = e.cbutton.button;
                bool ch = false;
                if (bed.waiting()) {
                    if (bed.col() == 2) ch = bed.on_pad_button(b);
                } else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) bed.move(-1, 0);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) bed.move(1, 0);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT) bed.move(0, -1);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) bed.move(0, 1);
                else if (b == SDL_CONTROLLER_BUTTON_A) bed.select();
                else if (b == SDL_CONTROLLER_BUTTON_X) ch = bed.clear();
                else if (b == SDL_CONTROLLER_BUTTON_B) {
                    if (bed.back()) {
                        if (bed_from_hotkey) close_menu();
                        else set_title("PacArcadeComp - MENU");
                    }
                }
                if (ch) {
                    bind = bed.bindings();
                    save_controls();
                }
                if (!bed.message().empty()) say("Controls: " + bed.message());
                force_redraw = true;
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN && mn.is_open()) { // the pad works the menu: d-pad moves, A selects, B goes back, the MENU button closes it
                int b = e.cbutton.button;
                if (controls::pad_command_for(bind, b) == controls::PadCommand::Menu) do_cmd(menu::Cmd::Resume, 1);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) menu_key(menu::Key::Up);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) menu_key(menu::Key::Down);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT) menu_key(menu::Key::Left);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) menu_key(menu::Key::Right);
                else if (b == SDL_CONTROLLER_BUTTON_A) menu_key(menu::Key::Enter);
                else if (b == SDL_CONTROLLER_BUTTON_B) menu_key(menu::Key::Back);
            } else if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && !bed.active() && !mn.is_open() && !o.scripted &&
                       !replay_on) { // S34B: system commands on the pad
                controls::PadCommand pc = controls::pad_command_for(bind, e.cbutton.button);
                bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
                if (pc == controls::PadCommand::Rewind) {
                    pad_rewind_down = down;
                    if (down && !extras) flash("rewind is switched off in Purist mode");
                } else if (down && pc == controls::PadCommand::Menu) open_menu();
                else if (down && pc == controls::PadCommand::Pause) {
                    paused = !paused;
                    set_title(paused ? "PacArcadeComp - PAUSED (press the pause button again)" : game_title());
                    say(paused ? "State: paused" : "State: resumed");
                    flash_until = 0;
                    force_redraw = true;
                } else if (down && pc == controls::PadCommand::Save) do_cmd(menu::Cmd::SaveState, 1);
                else if (down && pc == controls::PadCommand::Load) do_cmd(menu::Cmd::LoadState, 1);
            } else if ((e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL) && mn.is_open()) { // X04/X05: the mouse in the menu
                int ww2, wh2;
                SDL_GetWindowSize(win, &ww2, &wh2);
                display::Layout l2 = display::compute_layout(ww2, wh2, overlay::kOutW, H, purist ? display::Fit::Integer : fit);
                int mx = 0, my = 0, px = 0, py = 0;
                SDL_GetMouseState(&mx, &my);
                bool inside = menu::window_to_picture(l2.dest.x, l2.dest.y, l2.dest.w, l2.dest.h, overlay::kOutW, H, mx, my, &px, &py);
                int wheel = e.type == SDL_MOUSEWHEEL ? (e.wheel.y > 0 ? -1 : e.wheel.y < 0 ? 1 : 0) * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1) : 0;
                if (bed.active()) { // X05: the Bindings screen
                    int br = 0, bc = 0;
                    if (e.type == SDL_MOUSEWHEEL) {
                        if (wheel && !bed.waiting()) {
                            bed.move(wheel, 0);
                            say("Controls: mouse wheel to row " + std::to_string(bed.row()));
                            force_redraw = true;
                        }
                    } else if (e.type == SDL_MOUSEMOTION) {
                        if (inside && !bed.waiting() && menu::bindings_cell_at(px, py, &br, &bc) && (br != bed.row() || bc != bed.col())) {
                            bed.point(br, bc);
                            force_redraw = true;
                        }
                    } else if (e.button.button == SDL_BUTTON_LEFT) {
                        if (inside && menu::bindings_cell_at(px, py, &br, &bc)) {
                            bed.click_cell(br, bc);
                            say("Controls: mouse click on row " + std::to_string(br) + ", column " + std::to_string(bc) + (bed.waiting() ? " (waiting for a key)" : ""));
                            if (!bed.message().empty()) say("Controls: " + bed.message());
                            force_redraw = true;
                        }
                    } else if (e.button.button == SDL_BUTTON_RIGHT) {
                        say(bed.waiting() ? "Controls: mouse right click (cancelled)" : "Controls: mouse right click (back)");
                        if (bed.back()) {
                            if (bed_from_hotkey) close_menu();
                            else set_title("PacArcadeComp - MENU");
                        }
                        force_redraw = true;
                    }
                } else if (e.type == SDL_MOUSEWHEEL) {
                    if (wheel) {
                        menu_key(wheel < 0 ? menu::Key::Up : menu::Key::Down);
                        say("Menu: mouse wheel to item " + std::to_string(mn.cursor()) + " of page " + std::to_string(mn.page()));
                        force_redraw = true;
                    }
                } else {
                    if (e.type == SDL_MOUSEMOTION) {
                        if (inside && mn.hover(px, py)) {
                            force_redraw = true;
                            say("Menu: mouse on item " + std::to_string(mn.cursor()) + " of page " + std::to_string(mn.page()));
                        }
                    } else if (e.button.button == SDL_BUTTON_LEFT) {
                        if (inside && menu::help_pin_at(px, py)) {
                            fill_model();
                            if (menu::locked(menu::Cmd::HelpPin, model)) say("Menu: the key panel pin is LOCKED in Purist mode");
                            else {
                                do_cmd(menu::Cmd::HelpPin, 1);
                                say(std::string("Menu: mouse click on the key panel header: panel ") + (help_pin ? "pinned" : "unpinned"));
                            }
                            force_redraw = true;
                        } else if (inside) {
                            fill_model();
                            const bool on_item = mn.item_at(px, py) >= 0;
                            menu::Result r = mn.click(px, py, model);
                            if (on_item) say("Menu: mouse click on item " + std::to_string(mn.cursor()) + " (page " + std::to_string(mn.page()) + ")");
                            if (r.cmd != menu::Cmd::None) do_cmd(r.cmd, r.delta);
                            force_redraw = true;
                        }
                    } else if (e.button.button == SDL_BUTTON_RIGHT) {
                        say("Menu: mouse right click (back)");
                        menu_key(menu::Key::Back);
                        force_redraw = true;
                    }
                }
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (bed.active()) { // S34A: the bindings screen has the keyboard
                    SDL_Keycode k = e.key.keysym.sym;
                    int sc = (int)e.key.keysym.scancode;
                    bool ch = false;
                    if (k == SDLK_ESCAPE) {
                        if (bed.back()) {
                            if (bed_from_hotkey) close_menu();
                            else set_title("PacArcadeComp - MENU");
                        }
                    } else if (k == SDLK_BACKSPACE || k == SDLK_DELETE) ch = bed.clear();
                    else if (bed.waiting()) ch = bed.on_key(sc);
                    else if (k == SDLK_UP || k == SDLK_w) bed.move(-1, 0);
                    else if (k == SDLK_DOWN || k == SDLK_s) bed.move(1, 0);
                    else if (k == SDLK_LEFT || k == SDLK_a) bed.move(0, -1);
                    else if (k == SDLK_RIGHT || k == SDLK_d) bed.move(0, 1);
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) bed.select();
                    if (ch) {
                        bind = bed.bindings();
                        save_controls();
                    }
                    if (!bed.message().empty()) say("Controls: " + bed.message());
                    force_redraw = true;
                } else if (mn.is_open() &&
                           (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_LEFT ||
                            e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_a ||
                            e.key.keysym.sym == SDLK_d || e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER || e.key.keysym.sym == SDLK_SPACE)) {
                    SDL_Keycode k = e.key.keysym.sym;
                    menu_key(k == SDLK_ESCAPE                   ? menu::Key::Back
                             : (k == SDLK_UP || k == SDLK_w)    ? menu::Key::Up
                             : (k == SDLK_DOWN || k == SDLK_s)  ? menu::Key::Down
                             : (k == SDLK_LEFT || k == SDLK_a)  ? menu::Key::Left
                             : (k == SDLK_RIGHT || k == SDLK_d) ? menu::Key::Right
                                                                : menu::Key::Enter);
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (o.scripted || replay_on) running = false; // unattended runs: Esc closes the window as before
                    else open_menu();
                } else if (e.key.keysym.sym == SDLK_F9) do_cmd(menu::Cmd::Keys, 1);
                else if (e.key.keysym.sym == SDLK_F10) do_cmd(menu::Cmd::Pad, 1);
                else if (!o.scripted && e.key.keysym.sym == SDLK_F1) {
                    if (mn.is_open()) do_cmd(menu::Cmd::Resume, 1);
                    else {
                        paused = !paused;
                        set_title(paused ? "PacArcadeComp - PAUSED (F1 resumes)" : game_title());
                        say(paused ? "State: paused" : "State: resumed");
                        flash_until = 0;
                        force_redraw = true;
                    }
                } else if (!o.scripted && e.key.keysym.sym == SDLK_F2) do_cmd(menu::Cmd::SaveState, 1);
                else if (!o.scripted && e.key.keysym.sym == SDLK_F4) do_cmd(menu::Cmd::LoadState, 1);
                else if (e.key.keysym.sym == SDLK_F12) do_cmd(menu::Cmd::ResetControls, 1);
                else handle_key(e.key.keysym.sym);
            }
        }
        for (auto &st : stalls)
            if (!st.done && (long)ticks >= st.tick) {
                st.done = true;
                say("Test: freezing for " + std::to_string(st.ms) + " ms at tick " + std::to_string(ticks));
                SDL_Delay((Uint32)st.ms);
            }
        for (auto &k : keyat)
            if (!k.done && (long)ticks >= k.tick) {
                k.done = true;
                handle_key(k.key);
            }
        for (auto &ca : cmdat)
            if (!ca.done && (long)ticks >= ca.tick) {
                ca.done = true;
                say("Test: menu command at tick " + std::to_string(ticks));
                do_cmd(ca.cmd, 1);
            }
        for (auto &va : vat)
            if (!va.done && (long)(SDL_GetTicks() - t_start_ms) >= va.ms) {
                va.done = true;
                SDL_Joystick *vj = pad ? SDL_GameControllerGetJoystick(pad) : nullptr;
                if (vj) {
                    if (va.axis) SDL_JoystickSetVirtualAxis(vj, va.index, (Sint16)va.value);
                    else SDL_JoystickSetVirtualButton(vj, va.index, (Uint8)va.value);
                }
            }
        for (auto &pa : padat)
            if (!pa.done && (long)(SDL_GetTicks() - t_start_ms) >= pa.tick) {
                pa.done = true;
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = pa.down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
                ev.cbutton.button = (Uint8)pa.button;
                ev.cbutton.state = pa.down ? SDL_PRESSED : SDL_RELEASED;
                SDL_PushEvent(&ev);
            }
        uint64_t now = SDL_GetPerformanceCounter();
        {
            static uint64_t last_loop = 0;
            if (last_loop && (double)(now - last_loop) / pf > 0.05 && ticks > 120)
                say("Note: the program was frozen for " + std::to_string((int)(1000.0 * (double)(now - last_loop) / pf)) + " ms before machine tick " +
                    std::to_string(ticks) + " (the sound queue is re-set after a freeze)");
            last_loop = now;
        }
        int steps = 0;
        if (extras) {
            bool bs_bound = false;
            for (int a = 0; a < controls::kActions; ++a)
                for (int i = 0; i < 2; ++i)
                    if (bind.key[a][i] == SDL_SCANCODE_BACKSPACE) bs_bound = true;
            bool was_rewinding = rewinding;
            rewinding = extras && !paused && !bed.active() && ((!bs_bound && SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_BACKSPACE]) || pad_rewind_down);
            if (rewinding && !was_rewinding) {
                rewound_frames = 0;
                say("State: rewinding (release Backspace or the pad button to play on)");
            }
            if (!rewinding && was_rewinding)
                say("State: rewind stopped " + std::to_string(rewound_frames) +
                    " frames back (what you score from now on in this game will not count for the high score)");
            if (keeper.rule().mask() != last_assist_mask) { // X03: the running game stopped (or started again to) count: say why, once
                last_assist_mask = keeper.rule().mask();
                if (last_assist_mask) say(keeper.rule().sentence());
                if (!flash_until && !paused && !mn.is_open() && !bed.active()) set_title(game_title());
            }
            if (flash_until && SDL_GetTicks() > flash_until) {
                flash_until = 0;
                if (!bed.active()) set_title(focus_paused ? "PacArcadeComp - PAUSED (PRESS ANY KEY)" : paused ? "PacArcadeComp - PAUSED (F1 resumes)" : game_title());
            }
        }
        steps_this_pass = 0;
        if (paused) {
            next = (double)now + period;
        } // frozen: nothing runs and no lost time is made up on resume
        else if (rewinding) {
            if (smooth) {
                if (presents % (uint64_t)smooth_n == 0) {
                    rewind_tick();
                    next = (double)now + period;
                }
            } else {
                int n = 0;
                while ((double)now >= next && n < 5) {
                    rewind_tick();
                    next += period;
                    ++n;
                }
                if ((double)now >= next) next = (double)now + period;
            } // backwards at the normal speed
        } else if (o.fast) {
            for (int i = 0; i < 30 && running; ++i) one_tick();
        } // S34 test aid: no waiting (the hashes do not depend on timing)
        else if (smooth) {
            // S36 bug fix: a tick on every smooth_n-th refresh, but never faster than the clock allows. When the screen timing runs wild (a window resize on Windows can make the Direct3D screen present without waiting for the refresh) the game used to run fast and flood the sound queue (0.7 s of delay, wrong pitch, crackling).
            ++smooth_since;
            const double sp = (double)pf / smooth_hz;
            if (smooth_since >= smooth_n && (double)now >= smooth_next - 0.5 * sp) {
                smooth_since = 0;
                smooth_next += sp;
                if (smooth_next < (double)now - sp) smooth_next = (double)now; // after a freeze do not make up the lost time
                one_tick();
            }
        } else {
            bool burst = false;
            while ((double)now >= next) {
                burst = burst || (double)now >= next + period; // more than a whole tick behind: a stall happened; this and the rest of this pass are catch-up frames
                sound_catching_up = burst;
                one_tick();
                next += period;
                if (steps_this_pass >= 5) {
                    next = (double)now + period;
                    break;
                } // after a long stall (window drag, breakpoint) do not try to catch up
            }
        }
        steps = steps_this_pass;
        if (paused || rewinding || mn.is_open() || bed.active()) {
            hx.invalidate();
            evr.invalidate();
            stc.invalidate();
            lbd.discontinuity();
            haptics::Out z;
            if (rumble_last != z) rumble_send(z);
        } // nothing shakes while the game is paused, rewinding or in a menu
        if (paused || mn.is_open() || bed.active() || focus_paused) lb_step(true); // X09: the menu colour (no machine frames run then)
        {
            static uint32_t last_toast = 0;
            if (toast_until != last_toast) {
                last_toast = toast_until;
                force_redraw = true;
            }
        } // a new notice is drawn at once
        if (toast_until && SDL_GetTicks() >= toast_until) {
            toast_until = 0;
            force_redraw = true;
        }
        if (steps || force_redraw || mn.is_open()) {
            force_redraw = false;
            compose(g, roms, m, o, fb);
            const uint32_t *srcpix = over;
            static uint32_t wide[overlay::kOutW * overlay::kOutH];
            if (overlay_on && !purist && !mn.is_open()) overlay::compose(g, m.bus, fb, over); // reads memory only; the emulated picture in fb is not changed
            else {
                for (int y = 0; y < H; ++y) { std::memcpy(wide + y * TW, fb + y * W, W * 4); }
                srcpix = wide; // the 224-wide picture, in the 352-wide texture
                if (mn.is_open() && bed.active()) menu::draw_bindings(g, wide, bed, pad != nullptr || joy != nullptr, pad_type_now());
                else if (mn.is_open()) {
                    fill_model();
                    mn.draw(g, wide, model);
                    menu::draw_help(g, wide, model);
                } else if (help_pin) {
                    fill_model();
                    menu::draw_help(g, wide, model);
                }
            }
            if (focus_paused && !mn.is_open()) menu::draw_focus_pause(g, const_cast<uint32_t *>(srcpix)); // X06
            {
                const bool pop_ok = !mn.is_open() && !paused && !focus_paused && m.bus.read(0x4E00) == 3 &&
                                    m.bus.read(0x4E04) == 3; // X25: not on the READY screen (4E04 below 3), not paused, no menu
                const int pid = popup.update(pop_ok, SDL_GetTicks());
                if (pid >= 0) menu::draw_achievement_popup(g, const_cast<uint32_t *>(srcpix), pid);
            }
            if (best_until && !mn.is_open()) {
                if (SDL_GetTicks() > best_until) best_until = 0;
                else menu::draw_new_best(g, const_cast<uint32_t *>(srcpix), best_l1, best_l2);
            } // X24
            if (summary_until && !mn.is_open()) {
                if (SDL_GetTicks() > summary_until) summary_until = 0;
                else menu::draw_run_summary(g, const_cast<uint32_t *>(srcpix), summary_data);
            } // X22
            if (!mn.is_open() && toast_until && SDL_GetTicks() < toast_until)
                menu::draw_toast(g, const_cast<uint32_t *>(srcpix), toast_type, toast_connected); // S34C: pad plugged in / removed
            SDL_UpdateTexture(tex, nullptr, srcpix, TW * 4);
            SDL_UpdateTexture(texl, nullptr, srcpix, TW * 4);
            if (surround_mode == surround::kAmbient && !purist) surround::ambient_grid(srcpix, TW, W, H, surround::kAmbientW, surround::kAmbientH, amb_pix.data()); // X46
            if (glow_level && !purist) { // X41: the glow picture of this finished picture (the arcade part only), a texture of its own
                if (!glow_tex) {
                    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear", SDL_HINT_OVERRIDE);
                    glow_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
                    if (glow_tex) SDL_SetTextureBlendMode(glow_tex, SDL_BLENDMODE_ADD);
                }
                if (glow_tex) {
                    crt::glow(srcpix, TW, W, H, glow_level, glow_pix.data());
                    SDL_UpdateTexture(glow_tex, nullptr, glow_pix.data(), W * 4);
                }
            }
            ++shown;
        }
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        const int cw = panel_on() ? overlay::kOutW : W;
        display::Layout lay = display::compute_layout(ww, wh, cw, H, purist ? display::Fit::Integer : fit);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        {
            static int lw = -1, lh = -1, lx = -1, ly = -1, ldw = -1, ldh = -1;
            if (ww != lw || wh != lh || lay.dest.x != lx || lay.dest.y != ly || lay.dest.w != ldw || lay.dest.h != ldh) {
                lw = ww;
                lh = wh;
                lx = lay.dest.x;
                ly = lay.dest.y;
                ldw = lay.dest.w;
                ldh = lay.dest.h;
                say("Layout: window " + std::to_string(ww) + "x" + std::to_string(wh) + ", picture " + std::to_string(lx) + "," + std::to_string(ly) + " " +
                    std::to_string(ldw) + "x" + std::to_string(ldh));
            }
        }
        if (surround_mode == surround::kAmbient && !purist &&
            (lay.dest.w < ww ||
             lay.dest.h < wh)) { // X46: a blurred copy of the picture (darkened while it is averaged) over the whole window (only where there is room around the picture)
            if (!amb_tex) {
                SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear", SDL_HINT_OVERRIDE);
                amb_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, surround::kAmbientW, surround::kAmbientH);
            }
            if (amb_tex) {
                SDL_UpdateTexture(amb_tex, nullptr, amb_pix.data(), surround::kAmbientW * 4);
                const surround::Rect cs = surround::cover_source(surround::kAmbientW, surround::kAmbientH, ww, wh);
                const SDL_Rect asrc{cs.x, cs.y, cs.w, cs.h};
                SDL_RenderCopy(ren, amb_tex, &asrc, nullptr);
            }
        }
        if (surround_mode == surround::kCabinet && !purist) { // X44/X45: the background layer, before the picture (the picture's place and size are not changed)
            const surround::Rect pr{lay.dest.x, lay.dest.y, lay.dest.w, lay.dest.h};
            if (own_surround.w > 0) { // the player's own picture, scaled to cover the window
                if (!sur_tex) {
                    sur_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, own_surround.w, own_surround.h);
                    if (sur_tex) {
                        SDL_UpdateTexture(sur_tex, nullptr, own_surround.argb.data(), own_surround.w * 4);
                        sur_w = -1;
#if SDL_VERSION_ATLEAST(2, 0, 12)
                        SDL_SetTextureScaleMode(sur_tex, SDL_ScaleModeLinear);
#endif
                    } else {
                        say("Surround: the picture could not be used on this graphics card (too large); using the generated cabinet");
                        own_surround = surround::Image();
                    }
                }
                if (sur_tex) {
                    const surround::Rect cs = surround::cover_source(own_surround.w, own_surround.h, ww, wh);
                    const SDL_Rect src{cs.x, cs.y, cs.w, cs.h};
                    SDL_RenderCopy(ren, sur_tex, &src, nullptr);
                }
            }
            if (own_surround.w <= 0) { // the generated cabinet: drawn once into a picture and redrawn only when the window or the picture rectangle changes
                if (!sur_tex || sur_w != ww || sur_h != wh || sur_pr.x != pr.x || sur_pr.y != pr.y || sur_pr.w != pr.w || sur_pr.h != pr.h) {
                    if (sur_tex) SDL_DestroyTexture(sur_tex);
                    sur_w = ww;
                    sur_h = wh;
                    sur_pr = pr;
                    sur_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ww, wh);
                    if (sur_tex) {
                        std::vector<uint32_t> cp((size_t)ww * wh);
                        surround::render_cabinet(cp.data(), ww, wh, pr);
                        SDL_UpdateTexture(sur_tex, nullptr, cp.data(), ww * 4);
                    }
                }
                if (sur_tex) SDL_RenderCopy(ren, sur_tex, nullptr, nullptr);
            }
        }
        SDL_Rect src{0, 0, cw, H}, dst{lay.dest.x, lay.dest.y, lay.dest.w, lay.dest.h};
        const bool curved = curve_level && !purist && !mn.is_open(); // X42: flat while the menu is open, so that the mouse hits the right line
        {
            static int last_curved = -1;
            if (curve_level && !purist && last_curved != (int)curved) {
                last_curved = curved;
                say(curved ? "Curve: on (picture bent)" : "Curve: flat while the menu is open");
            }
        }
        const int cpw = (int)((long long)lay.dest.w * W / cw); // the width of the arcade picture on the screen (the overlay panel, if any, is to its right)
        auto draw_mesh = [&](SDL_Texture *t,
                             bool own_size) { // own_size: the glow texture is exactly W x H (the picture), the picture textures are TW x TH wide (picture + panel room)
            if (curve_built != curve_level || curve_dw != cpw || curve_dh != lay.dest.h || curve_dx != lay.dest.x || curve_dy != lay.dest.y) {
                curve_built = curve_level;
                curve_dw = cpw;
                curve_dh = lay.dest.h;
                curve_dx = lay.dest.x;
                curve_dy = lay.dest.y;
                crt::build_curve_mesh(curve_vert, curve_idx, 28, 36, lay.dest.x, lay.dest.y, cpw, lay.dest.h, crt::curve_k(curve_level), 0.f, 0.f, (float)W / TW,
                                      (float)H / TH);
            }
            const float us = own_size ? (float)TW / W : 1.f,
                        vs = own_size ? (float)TH / H : 1.f; // X41 fix: the glow texture has its own size, so its texture coordinates run 0 to 1
            std::vector<SDL_Vertex> sv(curve_vert.size());
            for (size_t i = 0; i < sv.size(); ++i) sv[i] = {{curve_vert[i].x, curve_vert[i].y}, {255, 255, 255, 255}, {curve_vert[i].u * us, curve_vert[i].v * vs}};
            SDL_RenderGeometry(ren, t, sv.data(), (int)sv.size(), curve_idx.data(), (int)curve_idx.size());
        };
        if (curved) {
            draw_mesh(texl, false);
            if (cw > W) {
                SDL_Rect psrc{W, 0, cw - W, H}, pdst{lay.dest.x + cpw, lay.dest.y, lay.dest.w - cpw, lay.dest.h};
                SDL_RenderCopy(ren, texl, &psrc, &pdst);
            }
        } else SDL_RenderCopy(ren, lay.integer_scale ? tex : texl, &src, &dst);
        if (curved && glow_level && glow_tex) draw_mesh(glow_tex, true);
        else if (glow_level && !purist && glow_tex) {
            const int pw = (int)((long long)lay.dest.w * W / cw);
            const SDL_Rect gdst{lay.dest.x, lay.dest.y, pw, lay.dest.h};
            SDL_RenderCopy(ren, glow_tex, nullptr, &gdst);
        } // X41: added onto the scaled picture
        if (scanlines && !purist && !curved && lay.scale >= 2.0) {
            // dark lines between the pixel columns of the arcade picture (the picture only, not the overlay panel), a quarter of a pixel wide
            std::vector<SDL_Rect> rr;
            rr.reserve(W);
            int lw = std::max(1, (int)(lay.scale / 4));
            for (int x = 0; x < W; ++x) rr.push_back({lay.dest.x + (int)((x + 1) * lay.scale) - lw, lay.dest.y, lw, lay.dest.h});
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 110);
            SDL_RenderFillRects(ren, rr.data(), (int)rr.size());
        }
        if (crt_mask &&
            !purist) { // X40: the shadow mask over the scaled picture (not over the overlay panel): a stripe picture of the size of the destination, multiplied onto it
            const int pw = (int)((long long)lay.dest.w * W / cw);
            const SDL_Rect mdst{lay.dest.x, lay.dest.y, pw, lay.dest.h}; // the picture only, not the overlay panel
            if (!crt_tex || crt_w != pw || crt_h != lay.dest.h || crt_level != crt_mask) {
                if (crt_tex) SDL_DestroyTexture(crt_tex);
                crt_w = pw;
                crt_h = lay.dest.h;
                crt_level = crt_mask;
                crt_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, crt_w, crt_h);
                if (crt_tex) {
                    std::vector<uint32_t> mk((size_t)crt_w * crt_h);
                    crt::fill(mk.data(), crt_w, crt_h, crt_mask);
                    SDL_UpdateTexture(crt_tex, nullptr, mk.data(), crt_w * 4);
                    SDL_SetTextureBlendMode(crt_tex, SDL_BLENDMODE_MOD);
                }
            }
            if (crt_tex) SDL_RenderCopy(ren, crt_tex, nullptr, &mdst);
        }
        if (curved) { // X42: the vignette (darker towards the edge), multiplied onto the picture part of the screen
            if (!vig_tex || vig_w != cpw || vig_h != lay.dest.h || vig_level != curve_level) {
                if (vig_tex) SDL_DestroyTexture(vig_tex);
                vig_w = cpw;
                vig_h = lay.dest.h;
                vig_level = curve_level;
                vig_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, vig_w, vig_h);
                if (vig_tex) {
                    std::vector<uint32_t> vp((size_t)vig_w * vig_h);
                    crt::vignette_fill(vp.data(), vig_w, vig_h, curve_level);
                    SDL_UpdateTexture(vig_tex, nullptr, vp.data(), vig_w * 4);
                    SDL_SetTextureBlendMode(vig_tex, SDL_BLENDMODE_MOD);
                }
            }
            if (vig_tex) {
                const SDL_Rect vdst{lay.dest.x, lay.dest.y, cpw, lay.dest.h};
                SDL_RenderCopy(ren, vig_tex, nullptr, &vdst);
            }
        }
        if (!o.shot.empty() && o.ticks_limit > 0 && (long)ticks >= o.ticks_limit) {
            int rw = 0, rh = 0;
            SDL_GetRendererOutputSize(ren, &rw, &rh);
            std::vector<uint32_t> px((size_t)rw * rh);
            if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, px.data(), rw * 4) == 0 && save_png_sized(o.shot, px.data(), rw, rh))
                say("Shot: wrote " + o.shot + " (" + std::to_string(rw) + "x" + std::to_string(rh) + ")");
            else say("Shot: could not write " + o.shot);
            o.shot.clear();
        }
        SDL_RenderPresent(ren);
        if (o.sim_refresh > 0) { // pretend the screen is o.sim_refresh Hz: wait for the next refresh
            if (burst_tick >= 0 && burst_end == 0 && (long)ticks >= burst_tick) {
                burst_end = (double)SDL_GetPerformanceCounter() + burst_ms * 0.001 * (double)pf;
                say("Test: the simulated screen runs without waiting for " + std::to_string(burst_ms) + " ms at tick " + std::to_string(ticks));
            }
            const bool in_burst =
                burst_end > 0 && (double)SDL_GetPerformanceCounter() < burst_end; // the screen runs 4 times too fast (not endless: the virtual test screen would choke)
            if (burst_end > 0 && !in_burst && !burst_over) {
                burst_over = true;
                sim_next = (double)SDL_GetPerformanceCounter();
            } // the burst is over: start the refresh rhythm again from now
            sim_next += (double)pf / o.sim_refresh / (in_burst ? 4.0 : 1.0);
            while ((double)SDL_GetPerformanceCounter() < sim_next) {
                if ((sim_next - (double)SDL_GetPerformanceCounter()) > pf * 0.002) SDL_Delay(1);
            }
        } else if (!vsync) SDL_Delay(1);
        uint64_t t_now = SDL_GetPerformanceCounter();
        pst.ticks = ticks;
        pst.present(steps > 0, 1000.0 * (double)(t_now - last_present) / pf);
        last_present = t_now;
        ++presents;
        if (o.ticks_limit > 0 && (long)ticks >= o.ticks_limit) running = false;
        if (o.measure > 0 && (double)(SDL_GetPerformanceCounter() - start) / pf >= o.measure) running = false;
    }
    if (extras) {
        keeper.finish(m);
        std::string msg = keeper.take_message();
        if (!msg.empty()) say(msg);
    } // S33: a game still running counts if it was not assisted
    double secs = (double)(SDL_GetPerformanceCounter() - start) / pf;
    if (!o.dump_end.empty()) save_png(o.dump_end, fb); // the last picture shown (for scripted window tests)
    {
        char st[200];
        int sc = 0;
        for (int i = 2; i >= 0; --i) {
            int b = m.bus.read(0x4E80 + i);
            sc = sc * 100 + (b >> 4) * 10 + (b & 15);
        }
        std::snprintf(st, sizeof st, "Machine at exit: main state %d, lives %d, dots eaten %d, score %d, credits %d.", m.bus.read(0x4E00), m.bus.read(0x4E14),
                      m.bus.read(0x4E0E), sc, m.bus.read(0x4E6E));
        say(st);
    }
    rumble_stop();
    {
        lightbar::Rgb c;
        if (lbd.restore(pad && SDL_GameControllerHasLED(pad) == SDL_TRUE, &c)) {
            SDL_GameControllerSetLED(pad, c.r, c.g, c.b);
            say("Light bar: " + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + " (the neutral blue, put back on exit)");
        }
    }
    if (!o.stats_file.empty()) { // X22
        if (purist) say("Stats: off in Purist mode");
        else {
            stats_log += records_log;
            for (int p = 0; p < 2; ++p)
                if (stc.run(p).active) stats_line("ACTIVE", p + 1, stc.run(p), state::score_ram(m.bus, p), (unsigned)ticks);
            FILE *sf = std::fopen(o.stats_file.c_str(), "w");
            if (sf) {
                std::fputs(stats_log.c_str(), sf);
                std::fclose(sf);
                say("Stats: wrote " + o.stats_file);
            } else say("Cannot write " + o.stats_file);
        }
    }
    if (!o.events_file.empty()) {
        if (purist) say("Events: off in Purist mode");
        else if (evr.write(o.events_file)) {
            std::string t;
            for (int k = 0; k < events::kTypeCount; ++k) t += std::string(k ? ", " : "") + events::name(k) + " " + std::to_string(evr.count(k));
            say("Events summary: " + t);
        } else say("Cannot write " + o.events_file);
    }
    if (o.virtual_pad)
        say("Test pad light bar received: " + std::to_string((int)vl_calls) + " colour commands, last " + std::to_string((int)vl_last[0]) + "," +
            std::to_string((int)vl_last[1]) + "," + std::to_string((int)vl_last[2]));
    if (pad && pad_rumble) {
        std::string ev;
        for (int f = 0; f < haptics::kFxCount; ++f) ev += std::string(f ? ", " : "") + haptics::name(f) + " " + std::to_string(rumble_events[f]);
        say("Vibration summary: " + std::to_string(rumble_cmds) + " motor commands sent, strongest big motor " + std::to_string((int)(rumble_peak_low * 100)) +
            "%, small motor " + std::to_string((int)(rumble_peak_high * 100)) + "%, triggers " + std::to_string((int)(rumble_peak_trig * 100)) + "%; effects: " + ev);
        say("Test pad received: " + std::to_string((int)vr_calls) + " motor commands (strongest big " + std::to_string((int)vr_low_max) + ", small " +
            std::to_string((int)vr_high_max) + " of 65535), " + std::to_string((int)vr_trig_calls) + " trigger-motor commands (strongest " +
            std::to_string((int)vr_trig_max) + ")");
    }
    char line[400];
    std::snprintf(line, sizeof line, "Ran %.2f s: %llu machine ticks, %llu pictures drawn", secs, (unsigned long long)ticks, (unsigned long long)shown);
    say(line);
    std::snprintf(line, sizeof line, "Measured tick rate: %.3f per second (arcade: %.3f)", ticks / secs, TICK_HZ);
    say(line);
    say(pst.report());
    {
        char hb[120];
        std::snprintf(hb, sizeof hb, "Machine hash after %llu ticks: %016llx", (unsigned long long)ticks, (unsigned long long)display::memory_hash(m.bus));
        say(hb);
    }
    if (adev) {
        SDL_PauseAudioDevice(adev, 1);
        SDL_CloseAudioDevice(adev);
        double ms = 1000.0 / audio_rate;
        uint64_t lc = astats.level_count.load();
        double avg = lc ? (double)astats.level_sum.load() / lc * ms : 0, mx = (double)astats.level_max.load() * ms, mn = lc ? (double)astats.level_min.load() * ms : 0,
               dev = audio_buffer * ms;
        std::snprintf(line, sizeof line, "Audio: %llu frames of sound made, %llu callbacks, %llu underruns (%llu samples missing), %llu samples dropped (queue full)",
                      (unsigned long long)audio_frames, (unsigned long long)astats.callbacks.load(), (unsigned long long)astats.underruns.load(),
                      (unsigned long long)astats.missing_samples.load(), (unsigned long long)astats.dropped_samples.load());
        say(line);
        std::snprintf(line, sizeof line,
                      "Audio pitch: resampler scale smallest %.4f, largest %.4f; %llu of %llu sound frames more than 0.5 %% off nominal, %llu more than 1 %% off", sc_min,
                      sc_max, (unsigned long long)frames_off, (unsigned long long)audio_frames, (unsigned long long)frames_off1);
        say(line);
        std::snprintf(line, sizeof line, "Audio guard: %llu times the queue was re-set after a dry card or a burst, %llu sound frames dropped as too late",
                      (unsigned long long)guard_resyncs, (unsigned long long)guard_dropped);
        say(line);
        std::snprintf(line, sizeof line,
                      "Audio delay: device buffer %.1f ms + queue (average %.1f ms, smallest %.1f, largest %.1f) = about %.1f ms on average, at most %.1f ms", dev, avg,
                      mn, mx, dev + avg, dev + mx);
        say(line);
        if (!o.audio_capture.empty()) {
            FILE *wf = std::fopen(o.audio_capture.c_str(), "wb");
            if (wf) {
                auto u32 = [&](uint32_t v) {
                    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
                    std::fwrite(b, 1, 4, wf);
                };
                auto u16 = [&](uint16_t v) {
                    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
                    std::fwrite(b, 1, 2, wf);
                };
                uint32_t bytes = (uint32_t)actx.capture.size() * 2;
                std::fwrite("RIFF", 1, 4, wf);
                u32(36 + bytes);
                std::fwrite("WAVEfmt ", 1, 8, wf);
                u32(16);
                u16(1);
                u16(1);
                u32(audio_rate);
                u32(audio_rate * 2);
                u16(2);
                u16(16);
                std::fwrite("data", 1, 4, wf);
                u32(bytes);
                std::fwrite(actx.capture.data(), 2, actx.capture.size(), wf);
                std::fclose(wf);
                say("Saved the audio handed to the sound card: " + o.audio_capture);
            }
        }
    }
    if (hashlog) std::fclose(hashlog);
    if (pad) SDL_GameControllerClose(pad);
    if (joy) SDL_JoystickClose(joy);
    if (crt_tex) { SDL_DestroyTexture(crt_tex); }
    if (glow_tex) SDL_DestroyTexture(glow_tex);
    if (sur_tex) SDL_DestroyTexture(sur_tex);
    if (amb_tex) SDL_DestroyTexture(amb_tex);
    if (vig_tex) SDL_DestroyTexture(vig_tex);
    SDL_DestroyTexture(tex);
    SDL_DestroyTexture(texl);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    write_report();
    return 0;
}
