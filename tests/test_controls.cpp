// S32 test of src/app/controls.{h,cpp}. Usage: test_controls pacman.zip
// A: standard bindings; text -> parse round trip; settings written to a file and read back by a "restarted" program (a fresh object)
// B: bad lines are reported and ignored, reserved keys refused, missing file = defaults
// C: the remap dialogue (keys, pad): sequence, one key one control, Esc cancels with no change, Backspace = none on the pad
// D: pad announcement is given once (the S03 double line)
// E: one-button Start on the real ROM: from the attract screen with no credit, ONE held Start gives a credit and starts the game;
//    only the Coin 1 and Start 1 bits are ever touched; nothing happens while a game runs or when the feature is off
// Build: see tests/test_controls.sh
#include <cstdio>
#include <cstdlib>
#include <string>
#include <SDL.h>
#include "../src/app/controls.h"
#include "../src/app/controls.cpp"
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
using namespace controls;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_controls pacman.zip\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }

    // ---- A
    Bindings d;
    expect(d.key[Up][0] == SDL_SCANCODE_UP && d.key[Up][1] == SDL_SCANCODE_W && d.key[Coin][0] == SDL_SCANCODE_5 && d.key[Start1][0] == SDL_SCANCODE_1 &&
               d.key[Start2][0] == SDL_SCANCODE_2,
           "standard keys are arrows/WASD, 5, 1, 2 (as before S32)");
    expect(d.pad[PadCoin] == SDL_CONTROLLER_BUTTON_BACK && d.pad[PadStart1] == SDL_CONTROLLER_BUTTON_START && d.pad[PadStart2] == -1 && d.one_button_start,
           "standard pad: Back = coin, Start = 1 player, one-button Start on");
    std::string problems;
    Bindings r = parse(to_text(d), &problems);
    expect(r == d && problems.empty(), "standard bindings -> text -> parse gives the same bindings, no problems");
    Bindings c = d;
    c.key[Up][0] = SDL_SCANCODE_I;
    c.key[Up][1] = 0;
    c.key[Coin][0] = SDL_SCANCODE_SPACE;
    c.pad[PadCoin] = SDL_CONTROLLER_BUTTON_X;
    c.pad[PadStart2] = SDL_CONTROLLER_BUTTON_Y;
    c.one_button_start = false;
    expect(parse(to_text(c), &problems) == c && problems.empty(), "changed bindings survive text -> parse");
    std::string path = "build/test_controls_dir/PacmanPort/controls.cfg";
    std::remove(path.c_str());
    Bindings back;
    std::string perr;
    expect(!load(path, back, &perr) && back == d, "no settings file: load says so and gives the standard controls");
    expect(save(path, c, &perr), "save creates the folder and the file (" + perr + ")");
    Bindings after_restart;
    bool had = load(path, after_restart, &perr); // a fresh object = what a restarted program does
    expect(had && after_restart == c && perr.empty(), "after a 'restart' the saved controls are read back exactly");
    expect(save(path, d, &perr) && load(path, back, &perr) && back == d, "saving again over the existing file works");
    {
        std::string p = default_config_path();
        expect(!p.empty() && p.find("controls.cfg") != std::string::npos, "default settings path found: " + p);
    }

    // ---- B
    Bindings bad = parse("up = Flurble, W\n coin = Escape\nfoo = 3\nnonsense\npad_coin = banana\none_button_start = maybe\nleft = Q\n", &problems);
    expect(bad.key[Up][0] == d.key[Up][0] && bad.key[Up][1] == SDL_SCANCODE_W, "unknown key name ignored, the other key of the line kept");
    expect(bad.key[Coin][0] == d.key[Coin][0], "Escape refused as a control key");
    expect(bad.pad[PadCoin] == d.pad[PadCoin] && bad.one_button_start, "unknown pad button / bad on-off ignored");
    expect(bad.key[Left][0] == SDL_SCANCODE_Q, "a good line after bad lines still applies");
    int nl = 0;
    for (char ch : problems)
        if (ch == '\n') ++nl;
    expect(nl == 6, "six problems reported (got " + std::to_string(nl) + ")");
    expect(key_is_reserved(SDL_SCANCODE_ESCAPE) && key_is_reserved(SDL_SCANCODE_F3) && key_is_reserved(SDL_SCANCODE_F12) && !key_is_reserved(SDL_SCANCODE_SPACE),
           "Esc and F keys reserved, Space is not");

    // ---- C
    {
        Remap rm;
        rm.begin(Remap::Keys, d);
        std::string msg;
        bool fin = false;
        int ks[7] = {SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, SDL_SCANCODE_RSHIFT};
        expect(rm.active() && rm.prompt().find("UP") != std::string::npos, "prompt names UP first");
        fin = rm.on_key(SDL_SCANCODE_F4, &msg);
        expect(!fin && msg.find("reserved") != std::string::npos && rm.prompt().find("UP") != std::string::npos, "an F key is refused and the same question stays");
        for (int i = 0; i < 7; ++i) {
            fin = rm.on_key(ks[i], &msg);
            expect(fin == (i == 6), "step " + std::to_string(i) + (i == 6 ? " finishes" : " continues"));
        }
        const Bindings &n = rm.result();
        expect(n.key[Up][0] == SDL_SCANCODE_I && n.key[Up][1] == 0 && n.key[Down][0] == SDL_SCANCODE_K && n.key[Left][0] == SDL_SCANCODE_J &&
                   n.key[Right][0] == SDL_SCANCODE_L && n.key[Coin][0] == SDL_SCANCODE_SPACE && n.key[Start1][0] == SDL_SCANCODE_RETURN &&
                   n.key[Start2][0] == SDL_SCANCODE_RSHIFT,
               "all seven keys assigned in order, the second key of each control cleared");
        expect(!rm.active() && !rm.cancelled(), "finished, not cancelled");
        Remap r2;
        r2.begin(Remap::Keys, d);
        r2.on_key(SDL_SCANCODE_I, &msg);
        r2.on_key(SDL_SCANCODE_I, &msg);
        expect(r2.result().key[Up][0] == 0 || r2.result().key[Down][0] == SDL_SCANCODE_I, "the same key given twice moves to the later control (one key, one control)");
        int uses = 0;
        for (int a = 0; a < kActions; ++a)
            for (int i = 0; i < 2; ++i)
                if (r2.result().key[a][i] == SDL_SCANCODE_I) ++uses;
        expect(uses == 1, "key I is bound exactly once");
        Remap r3;
        r3.begin(Remap::Keys, d);
        r3.on_key(SDL_SCANCODE_I, &msg);
        bool f3 = r3.on_key(SDL_SCANCODE_ESCAPE, &msg);
        expect(f3 && r3.cancelled() && !r3.active(), "Esc cancels");
        expect(d.key[Up][0] == SDL_SCANCODE_UP, "the old bindings object is untouched by a cancelled dialogue");
        Remap p;
        p.begin(Remap::Pad, d);
        expect(p.prompt().find("COIN") != std::string::npos, "pad dialogue asks for COIN first");
        p.on_pad_button(SDL_CONTROLLER_BUTTON_X, &msg);
        p.on_key(SDL_SCANCODE_BACKSPACE, &msg);
        bool fp = p.on_pad_button(SDL_CONTROLLER_BUTTON_Y, &msg);
        expect(fp && p.result().pad[PadCoin] == SDL_CONTROLLER_BUTTON_X && p.result().pad[PadStart1] == -1 && p.result().pad[PadStart2] == SDL_CONTROLLER_BUTTON_Y,
               "pad: coin = X, start 1 = none (Backspace), start 2 = Y");
        Remap p2;
        p2.begin(Remap::Pad, d);
        p2.on_pad_button(SDL_CONTROLLER_BUTTON_START, &msg); // Start was start 1: now coin; start 1 must lose it
        expect(p2.result().pad[PadCoin] == SDL_CONTROLLER_BUTTON_START && p2.result().pad[PadStart1] == -1, "a pad button used twice moves to the later control");
    }

    // ---- D
    {
        PadAnnouncer a;
        expect(a.should_announce("PS4 Controller", 100), "first announcement given");
        expect(!a.should_announce("PS4 Controller", 400), "the same pad again 300 ms later (SDL's 'added' event at start) is not announced");
        expect(a.should_announce("Xbox", 450), "a different pad is announced");
        a.forget();
        expect(a.should_announce("Xbox", 500), "after unplug (forget) the pad is announced again when re-plugged");
        expect(a.should_announce("Xbox", 5000), "the same pad much later is announced");
    }

    // ---- E: real ROM
    auto play = [&](bool enabled, int hold_from, int hold_to, int frames, int pre_credits, int *credits_out, int *state_out, int *extra_bits_out,
                    int *first_start_frame) {
        Machine m(roms.program);
        Inputs in;
        StartAssist sa;
        int bad = 0;
        if (first_start_frame) *first_start_frame = -1;
        for (int f = 0; f < frames; ++f) {
            bool held = f >= hold_from && f < hold_to;
            bool ac = false, as = false;
            if (pre_credits && f >= 400 && f < 405) in.set(Control::Coin1, true);
            else in.set(Control::Coin1, false);
            if (enabled) sa.update(held, m.bus.read(0x4E6E), m.bus.read(0x4E00), &ac, &as);
            if (ac) in.set(Control::Coin1, true);
            in.set(Control::Start1, held || as);
            in.apply(m.bus);
            // only these two bits may ever differ from idle: IN0 bit 5 (coin 1) and IN1 bit 5 (start 1)
            if ((in.in0() | 0x20) != 0xFF || (in.in1() | 0x20) != 0xFF) ++bad;
            if (first_start_frame && *first_start_frame < 0 && as) *first_start_frame = f;
            m.run_frame();
        }
        *credits_out = m.bus.read(0x4E6E);
        *state_out = m.bus.read(0x4E00);
        *extra_bits_out = bad;
    };
    int cr, st, nbad, fs;
    play(true, 300, 310, 700, 0, &cr, &st, &nbad, &fs);
    expect(st == 3, "one-button Start: one Start press from the attract screen starts a game (state 4E00 = " + std::to_string(st) + ")");
    expect(cr == 0, "the credit was used by the game (credits now " + std::to_string(cr) + ")");
    expect(nbad == 0, "only the Coin 1 and Start 1 input bits were ever used");
    play(false, 300, 310, 700, 0, &cr, &st, &nbad, &fs);
    expect(st == 1, "with one-button Start off, Start alone does nothing (state " + std::to_string(st) + ")");
    play(true, 5, 15, 800, 0, &cr, &st, &nbad, &fs);
    expect(st == 3 && nbad == 0, "a Start press while the ROM is still starting up (frame 5) is remembered and starts the game once the attract screen shows (state " +
                                     std::to_string(st) + ")");
    play(true, 500, 510, 800, 1, &cr, &st, &nbad, &fs);
    expect(st == 3 && fs < 0, "with a credit already in, the helper adds nothing (no helper start press: " + std::to_string(fs) + ")");
    {
        Machine m(roms.program);
        Inputs in;
        StartAssist sa;
        int credits_in_game = -1;
        for (int f = 0; f < 1500; ++f) {
            bool held = f >= 300 && f < 310;
            bool ac, as;
            sa.update(held || (f >= 900 && f < 910), m.bus.read(0x4E6E), m.bus.read(0x4E00), &ac, &as);
            in.set(Control::Coin1, ac);
            in.set(Control::Start1, held || as);
            in.apply(m.bus);
            m.run_frame();
            if (f == 1499) credits_in_game = m.bus.read(0x4E6E);
        }
        expect(credits_in_game == 0, "pressing Start again during the game adds no coin (credits " + std::to_string(credits_in_game) + ")");
    }

    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS test_controls", checks, failures);
    return failures ? 1 : 0;
}
