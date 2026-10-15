// S34A test of controls::BindEditor (src/app/controls.*): one command at a time, swap on conflict, clearing, reserved keys, pad column, save and reload.
#include <SDL.h>
#include <cstdio>
#include <string>
#include "../src/app/controls.h"
#include "../src/app/controls.cpp"
using namespace controls;
static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &w) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", w.c_str());
    }
}
// how many differences between two tables
static int diffs(const Bindings &a, const Bindings &b) {
    int n = 0;
    for (int i = 0; i < kActions; ++i)
        for (int j = 0; j < 2; ++j) n += a.key[i][j] != b.key[i][j];
    for (int i = 0; i < kPadActions; ++i) n += a.pad[i] != b.pad[i];
    return n;
}
int main() {
    Bindings def;
    BindEditor e;
    e.begin(def);
    expect(e.active() && !e.waiting() && e.row() == 0 && e.col() == 0, "opens browsing at the first slot");
    // A: change one command: only that slot changes
    e.move(4, 0);
    e.select();
    expect(e.waiting() && e.row() == Coin, "Enter on COIN key 1 waits for a key");
    expect(e.on_key(SDL_SCANCODE_C) && !e.waiting(), "a free key is taken");
    expect(e.bindings().key[Coin][0] == SDL_SCANCODE_C && diffs(def, e.bindings()) == 1, "exactly one slot changed");
    // B: swap on conflict (key already on another command)
    e.move(1, 0);
    e.select(); // START 1 key 1 (=1)
    expect(e.on_key(SDL_SCANCODE_C), "taking a used key is accepted");
    expect(e.bindings().key[Start1][0] == SDL_SCANCODE_C && e.bindings().key[Coin][0] == SDL_SCANCODE_1, "the two slots are swapped (COIN now has 1)");
    expect(e.message().find("SWAPPED") != std::string::npos && e.message().find("COIN") != std::string::npos, "the message says what was swapped: " + e.message());
    // C: no key is ever on two commands
    for (int a = 0; a < kActions; ++a)
        for (int i = 0; i < 2; ++i) {
            int k = e.bindings().key[a][i];
            if (!k) continue;
            int n = 0;
            for (int b = 0; b < kActions; ++b)
                for (int j = 0; j < 2; ++j)
                    if (e.bindings().key[b][j] == k) ++n;
            expect(n == 1, "key used once: " + key_name(k));
        }
    // D: same command, other slot -> slots swap
    BindEditor f;
    f.begin(def, Up, 0);
    f.select();
    expect(f.on_key(SDL_SCANCODE_W), "key from the other slot of the same command");
    expect(f.bindings().key[Up][0] == SDL_SCANCODE_W && f.bindings().key[Up][1] == SDL_SCANCODE_UP && diffs(def, f.bindings()) == 2,
           "slots swapped, nothing else changed");
    // E: reserved keys refused, stays waiting
    BindEditor g;
    g.begin(def, Left, 1);
    g.select();
    expect(!g.on_key(SDL_SCANCODE_ESCAPE) && g.waiting(), "Esc as a key is refused");
    expect(!g.on_key(SDL_SCANCODE_F5) && g.waiting(), "F5 is refused");
    expect(diffs(def, g.bindings()) == 0, "nothing changed by refused keys");
    expect(!g.back() && !g.waiting() && g.active() && g.message().find("CANCELLED") != std::string::npos, "Esc while waiting cancels the change only");
    expect(g.back() && !g.active(), "Esc while browsing closes the screen");
    // F: clear
    BindEditor h;
    h.begin(def, Down, 1);
    expect(h.clear() && h.bindings().key[Down][1] == 0 && diffs(def, h.bindings()) == 1, "Backspace clears one slot");
    expect(!h.clear() && h.message() == "ALREADY EMPTY", "clearing an empty slot changes nothing");
    h.move(0, -1);
    expect(h.col() == 0, "move left");
    h.clear();
    expect(h.message().find("NO KEY LEFT") != std::string::npos, "clearing the last key warns: " + h.message());
    // G: pad column: movement rows are fixed, coin/start rows take buttons; d-pad refused; swap
    BindEditor p;
    p.begin(def, Up, 2);
    p.select();
    expect(!p.waiting() && p.message().find("FIXED") != std::string::npos, "movement pad slot is fixed");
    expect(!p.clear(), "cannot clear a fixed slot");
    p.move(4, 0);
    expect(p.row() == Coin && p.col() == 2, "cursor on COIN pad");
    p.select();
    expect(p.waiting(), "waits for a pad button");
    expect(!p.on_key(SDL_SCANCODE_X) && p.waiting(), "a keyboard key is not accepted for the pad slot");
    expect(!p.on_pad_button(SDL_CONTROLLER_BUTTON_DPAD_UP) && p.waiting(), "d-pad refused");
    expect(p.on_pad_button(SDL_CONTROLLER_BUTTON_A) && p.bindings().pad[PadCoin] == SDL_CONTROLLER_BUTTON_A && diffs(def, p.bindings()) == 1, "A becomes COIN");
    p.move(1, 0);
    p.select();
    expect(p.on_pad_button(SDL_CONTROLLER_BUTTON_A), "taking A for START 1");
    expect(p.bindings().pad[PadStart1] == SDL_CONTROLLER_BUTTON_A && p.bindings().pad[PadCoin] == SDL_CONTROLLER_BUTTON_START, "pad buttons swapped (COIN got START)");
    // H: save and reload gives the same table
    Bindings t = p.bindings();
    t.key[Coin][0] = SDL_SCANCODE_C;
    std::string pr;
    Bindings back_ = parse(to_text(t), &pr);
    expect(pr.empty() && back_ == t, "to_text/parse round trip of an edited table");
    // I: the editor never touches one_button_start
    expect(p.bindings().one_button_start == def.one_button_start, "one_button_start untouched");
    // ---- S34B: the pad system commands
    {
        Bindings d2;
        expect(d2.pad[PadMenu] == SDL_CONTROLLER_BUTTON_RIGHTSTICK && d2.pad[PadPause] == -1 && d2.pad[PadSave] == -1 && d2.pad[PadLoad] == -1 && d2.pad[PadRewind] == -1,
               "defaults: MENU = right stick click, the other four unset");
        expect(d2.pad[PadCoin] == SDL_CONTROLLER_BUTTON_BACK && d2.pad[PadStart1] == SDL_CONTROLLER_BUTTON_START && d2.pad[PadStart2] == -1,
               "coin and start defaults unchanged");
        expect(BindEditor::kRows == 12, "twelve rows: 7 game commands and 5 system commands");
        expect(std::string(fixed_key_label(7)) == "ESC" && std::string(fixed_key_label(8)) == "F1" && std::string(fixed_key_label(9)) == "F2" &&
                   std::string(fixed_key_label(10)) == "F4" && std::string(fixed_key_label(11)) == "BACKSPACE" && std::string(fixed_key_label(3)) == "",
               "fixed keys of the system rows");
        expect(pad_command_for(d2, SDL_CONTROLLER_BUTTON_RIGHTSTICK) == PadCommand::Menu && pad_command_for(d2, SDL_CONTROLLER_BUTTON_BACK) == PadCommand::None &&
                   pad_command_for(d2, -1) == PadCommand::None && pad_command_for(d2, SDL_CONTROLLER_BUTTON_Y) == PadCommand::None,
               "a button maps to its system command only");
        BindEditor q;
        q.begin(d2, 8, 0);
        q.select();
        expect(!q.waiting() && q.message().find("FIXED") != std::string::npos, "the keyboard key of PAUSE is fixed (cannot be changed)");
        expect(!q.clear() && diffs(d2, q.bindings()) == 0, "a fixed key cannot be cleared");
        q.move(0, 1);
        q.select();
        expect(!q.waiting(), "key 2 of a system row is fixed too");
        q.move(0, 1);
        expect(q.col() == 2, "cursor on the pad column");
        q.select();
        expect(q.waiting(), "the pad column of PAUSE waits for a button");
        expect(q.on_pad_button(SDL_CONTROLLER_BUTTON_X) && q.bindings().pad[PadPause] == SDL_CONTROLLER_BUTTON_X && diffs(d2, q.bindings()) == 1,
               "X becomes PAUSE, nothing else changes");
        q.move(1, 0);
        q.select();
        expect(q.on_pad_button(SDL_CONTROLLER_BUTTON_X), "taking X for SAVE");
        expect(q.bindings().pad[PadSave] == SDL_CONTROLLER_BUTTON_X && q.bindings().pad[PadPause] == -1 && q.message().find("SWAPPED") != std::string::npos,
               "X moved from PAUSE to SAVE with a message: " + q.message());
    }
    { // a button already used for coin is swapped, never shared
        Bindings d3;
        BindEditor q;
        q.begin(d3, 11, 2);
        q.select();
        q.on_pad_button(SDL_CONTROLLER_BUTTON_BACK);
        expect(q.bindings().pad[PadRewind] == SDL_CONTROLLER_BUTTON_BACK && q.bindings().pad[PadCoin] == -1,
               "BACK given to REWIND leaves COIN empty (swap), never shared");
        for (int a = 0; a < kPadActions; ++a)
            for (int b2 = a + 1; b2 < kPadActions; ++b2) expect(q.bindings().pad[a] < 0 || q.bindings().pad[a] != q.bindings().pad[b2], "no pad button on two commands");
        expect(pad_command_for(q.bindings(), SDL_CONTROLLER_BUTTON_BACK) == PadCommand::Rewind, "BACK now means REWIND");
        expect(!q.on_pad_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT), "(not waiting any more: nothing happens)");
        q.select();
        expect(!q.on_pad_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) && q.waiting(), "the d-pad is refused for system commands too");
        q.clear();
        expect(q.bindings().pad[PadRewind] < 0, "clearing a system pad button");
    }
    { // the controls file: round trip, old files (three pad lines) still load, bad names ignored
        Bindings t;
        t.pad[PadPause] = SDL_CONTROLLER_BUTTON_X;
        t.pad[PadSave] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        t.pad[PadLoad] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        t.pad[PadRewind] = SDL_CONTROLLER_BUTTON_Y;
        t.pad[PadMenu] = -1;
        std::string pr2;
        Bindings r = parse(to_text(t), &pr2);
        expect(pr2.empty() && r == t && r.pad[PadMenu] == -1, "round trip of all eight pad commands (including MENU = none)");
        std::string old = "up = Up, W\npad_coin = back\npad_start1 = start\npad_start2 = none\none_button_start = on\n";
        Bindings o2 = parse(old, &pr2);
        expect(o2.pad[PadMenu] == SDL_CONTROLLER_BUTTON_RIGHTSTICK && o2.pad[PadCoin] == SDL_CONTROLLER_BUTTON_BACK && pr2.empty(),
               "an older file without the new lines loads and gets the defaults");
        std::string bad;
        Bindings b3 = parse("pad_save = banana\n", &bad);
        expect(!bad.empty() && b3.pad[PadSave] == -1, "an unknown button name is reported and ignored");
    }
    // ---- S34B fix: every button of an Xbox (XInput), DualShock 4 and DualSense pad can be bound, including L2/R2 (LT/RT), which SDL reports as axes
    {
        int bindable = 0;
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX + 0; ++b) {
            if (b >= SDL_CONTROLLER_BUTTON_DPAD_UP && b <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                expect(pad_button_is_dpad(b), "d-pad button " + std::to_string(b) + " is the d-pad");
                continue;
            }
            BindEditor e2;
            Bindings d4;
            e2.begin(d4, 9, 2);
            e2.select();
            bool ok = e2.on_pad_button(b);
            expect(ok && e2.bindings().pad[PadSave] == b, "button " + pad_name(b) + " (" + std::to_string(b) + ") can be bound to SAVE");
            Bindings t4 = e2.bindings();
            std::string pr4;
            Bindings r4 = parse(to_text(t4), &pr4);
            expect(pr4.empty() && r4 == t4 && r4.pad[PadSave] == b, "button " + pad_name(b) + " survives the controls file");
            expect(pad_command_for(t4, b) == PadCommand::Save, "button " + pad_name(b) + " maps to SAVE in the running game");
            if (ok) ++bindable;
        }
        expect(bindable == 21 - 4, "17 SDL buttons bindable (all but the four d-pad buttons): " + std::to_string(bindable));
        for (int tb : {kPadLeftTrigger, kPadRightTrigger}) {
            BindEditor e3;
            Bindings d5;
            e3.begin(d5, 8, 2);
            e3.select();
            expect(e3.on_pad_button(tb) && e3.bindings().pad[PadPause] == tb, "trigger " + pad_name(tb) + " can be bound to PAUSE");
            Bindings t5 = e3.bindings();
            std::string pr5;
            Bindings r5 = parse(to_text(t5), &pr5);
            expect(pr5.empty() && r5 == t5, "trigger survives the controls file: " + pad_name(tb));
            expect(pad_command_for(t5, tb) == PadCommand::Pause, "trigger maps to PAUSE");
            BindEditor e4;
            e4.begin(d5, 4, 2);
            e4.select();
            expect(e4.on_pad_button(tb) && e4.bindings().pad[PadCoin] == tb, "trigger can be bound to COIN too");
        }
        expect(pad_name(kPadLeftTrigger) == "lefttrigger" && pad_from_name("righttrigger") == kPadRightTrigger && pad_from_name("RightTrigger") == kPadRightTrigger,
               "trigger names in the file");
        BindEditor e5;
        Bindings d6;
        e5.begin(d6, 9, 2);
        e5.select();
        expect(!e5.on_pad_button(55) && e5.waiting(), "a button number that does not exist is ignored");
        // the trigger axes: pressed above 50 %, released below 25 %, rests at 0 (some pads rest negative)
        TriggerButtons tb2;
        std::vector<TriggerButtons::Change> ch;
        tb2.update(0, 0, ch);
        expect(ch.empty(), "triggers at rest: no change");
        tb2.update(10000, -3000, ch);
        expect(ch.empty(), "a light touch (30 %) is not a press");
        tb2.update(20000, 0, ch);
        expect(ch.size() == 1 && ch[0].button == kPadLeftTrigger && ch[0].down && tb2.left_down(), "left trigger pulled past 50 % = pressed");
        ch.clear();
        tb2.update(12000, 0, ch);
        expect(ch.empty() && tb2.left_down(), "half way back (37 %) stays pressed (no fluttering)");
        tb2.update(5000, 0, ch);
        expect(ch.size() == 1 && !ch[0].down && !tb2.left_down(), "released below 25 %");
        ch.clear();
        tb2.update(32767, 32767, ch);
        expect(ch.size() == 2 && tb2.right_down() && tb2.left_down(), "both triggers fully pulled");
        ch.clear();
        tb2.update(32767, 32767, ch);
        expect(ch.empty(), "holding gives no repeated presses");
        tb2.reset();
        expect(!tb2.left_down() && !tb2.right_down(), "reset releases both");
    }
    // J: nothing happens when not active
    BindEditor z;
    expect(!z.on_key(SDL_SCANCODE_A) && !z.clear() && !z.back(), "inactive editor ignores everything");
    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
