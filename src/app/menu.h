// S34: the pause menu (Esc), the key-help panel and the saved settings, plus the rules of Purist mode.
// The menu only decides WHAT the player asked for (it returns a command); the program (main.cpp) carries it out. Nothing here can touch the machine.
// Every setting that has a function key is also in the menu, and the help panel lists every key, so the two always agree.
// Drawing uses the debug overlay's helpers (overlay.cpp must be included before menu.cpp) and the game's own tile font: capital letters, digits, '-'.
#pragma once
#include "stats.h"
#include "achievements.h"
#include <cstdint>
#include <string>
#include <vector>
#include "../video/gfx.h"
#include "controls.h"
#include "lastdevice.h"

namespace menu {

enum class Cmd {
    None,
    Resume,
    Bindings,
    SaveState,
    LoadState,
    Lives,
    Bonus,
    Difficulty,
    Names,
    Restart,
    Keys,
    Pad,
    ResetControls,
    OneButton,
    Overlay,
    Scanlines,
    Pacing,
    Frame,
    Scaling,
    FullScreen,
    HelpPin,
    Purist,
    Quit,
    Vibration,
    PadSymbols,
    FocusPause,
    Hints,
    Lightbar,
    RunSummary,
    CrtMask,
    Glow,
    Curve,
    CrtLook,
    Surround,
    SaveSlot,
    LoadSlot,
    Rules,      // T02i: RULES ORIGINAL / READABLE
    WriteRules, // T02i: write the standard rules.cfg for modders
    Locked
};
enum class Key { Up, Down, Left, Right, Enter, Back };

// S34C: the kind of pad in use, for the icon and the button pictures. SDL_GameControllerType: 1 XBOX360, 2 XBOXONE, 3 PS3, 4 PS4, 7 PS5; anything else is "other" (plain names).
enum class PadType { None, Xbox, PS4, PS5, Other };
PadType pad_type_of_sdl(int sdl_controller_type, bool standard_pad);
const char *pad_type_word(PadType t); // "XBOX", "PS4", "PS5", "PAD", "" for none

// X12: pages 4 (SAVE STATE) and 5 (LOAD STATE) list the eight save slots; Enter on a slot returns Cmd::SaveSlot / LoadSlot with delta = the slot number 1..8.
// What the menu shows (the program fills it from its real state before every key and every drawing).
struct Model {
    bool purist = false, overlay = false, scanlines = false, smooth = false, frame = true, fill = false, fullscreen = false, one_button = true, help_pin = false;
    int vibration = 2;                  // S34R: 0 off, 1 light, 2 full
    bool pause_focus = true;            // X06: pause by itself when the window loses focus
    int hints = 0;                      // X08: 0 auto (follow the last device), 1 keyboard, 2 pad
    bool lightbar = true;               // X09: the pad's light bar shows yellow while playing, dim blue in the menu
    bool run_summary = true;            // X22: show the statistics page at game over
    int crt_mask = 0;                   // X40: 0 off, 1 light, 2 strong (the shadow mask)
    int glow = 0;                       // X41: 0 off, 1 light, 2 strong (the glow)
    int curve = 0;                      // X42: 0 off, 1 light, 2 strong (curve and vignette)
    int surround = 0;                   // X44: 0 black, 1 cabinet, 2 ambient (X46)
    int hint_device = 1;                // X08: lastdevice::Device of the last keyboard / pad use (1 keyboard, 3 pad)
    int pad_game_btn[3] = {-1, -1, -1}; // X08: pad buttons for COIN, START1, START2 (SDL button numbers, -1 = none)
    std::string key_cap[7][2];          // X08: the key names of the 7 game commands ("" = none), for the key caps
    int pad_symbols = 0;                // S34C: 0 auto, 1 Xbox, 2 PS4, 3 PS5 (which button pictures to draw)
    int lives = 2, bonus = 0;           // DipSettings::Lives (0 = 1 life, 1 = 2, 2 = 3, 3 = 5), DipSettings::Bonus (0 = 10000, 1 = 15000, 2 = 20000, 3 = none)
    bool diff_normal = true, names_normal = true;
    bool rules_readable = false;                // T02i: RULES READABLE chosen (the proved readable rules run instead of the ROM routines)
    bool rules_file = false;                    // T02i: a rules file with changed values is in use (the game is MODDED and uses the readable rules)
    std::string assist;                         // X03: label of the first reason this game is assisted ("" = the game counts)
    bool pending = false;                       // game options changed that need a restart of the machine
    PadType pad_type = PadType::None;           // S34C
    int pad_btn[5] = {-1, -1, -1, -1, -1};      // S34C: the same buttons as pad_name (SDL button numbers, -1 = none), for the pictures
    std::string pad_name[5];                    // S34B: pad button for MENU, PAUSE, SAVE, LOAD, REWIND ("" = none), from binding_lines()
    int ach_have[24] = {0}, ach_need[24] = {0}; // X29: progress of the achievements that have a count (need 0 = none), in the game that is running
    unsigned long long ach_mask = 0;            // X25: which achievements are unlocked (bit = achievements::Id)
    std::string rec_line[21];                   // X24: the Records page (records::line for levels 1 to 21)
    std::string slot_line[8];                   // X12: the eight save slots as the pickers show them (state::slot_line), "" = not filled in
    std::string bind_line[7];                   // S34A: one line per command for the help panel ("COIN  5"), from binding_lines()
};
struct Result {
    Cmd cmd = Cmd::None;
    int delta = 0;
}; // delta: +1 / -1 for a choice, +1 for a plain action

// Settings kept between runs (settings.cfg next to controls.cfg).
struct Settings {
    bool purist = false, overlay = false, scanlines = false, fill = false, smooth = false, frame = true, help_pin = false;
    int lives = 2, bonus = 0;
    bool diff_normal = true, names_normal = true;
    int vibration = 2;       // S34R: 0 off, 1 light, 2 full
    bool pause_focus = true; // X06
    int hints = 0;           // X08: 0 auto, 1 keyboard, 2 pad
    bool lightbar = true;    // X09
    bool run_summary = true; // X22
    int crt_mask = 0;        // X40: 0 off, 1 light, 2 strong
    int glow = 0;            // X41
    int curve = 0;           // X42
    int surround = 0;        // X44
    int pad_symbols = 0;     // S34C: 0 auto, 1 Xbox, 2 PS4, 3 PS5
    bool rules_readable = false; // T02i: key rules (original, readable)
};
bool load_settings(const std::string &path, Settings &s, std::string *problems); // false if there is no file
bool save_settings(const std::string &path, const Settings &s, std::string *error);

// Purist mode: the game runs exactly like the arcade board. These are the additions it switches off.
bool locked(Cmd c, const Model &m);
std::string value_text(Cmd c, const Model &m);
std::string label_of(Cmd c);

class Menu {
public:
    bool is_open() const { return open_; }
    void open() {
        open_ = true;
        page_ = 0;
        cur_[0] = 0;
    }
    void close() { open_ = false; }
    Result key(Key k, const Model &m);
    void draw(const Gfx &g, uint32_t *out, const Model &m) const; // dims the picture (left 224 columns of out) and draws the menu on it
    // X04: the mouse. Coordinates are in the 224 x 288 picture (see window_to_picture). item_at: the row under the pointer (-1 = none) and whether the pointer is
    // in the right half of the row. hover(): the row under the pointer becomes the selected one (true if that changed anything). click(): left click = Enter,
    // but on a choice item the left half steps it down and the right half up; right_click() = Esc (back / resume).
    int item_at(int px, int py, bool *right_half = nullptr) const;
    bool hover(int px, int py);
    Result click(int px, int py, const Model &m);
    Result right_click(const Model &m) { return key(Key::Back, m); }
    int page() const { return page_; }
    int cursor() const { return cur_[page_]; }
    std::string note;                                             // short message at the bottom (capital letters)
    std::vector<std::string> visible_lines(const Model &m) const; // the text of the current page (for tests)
private:
    bool open_ = false;
    int page_ = 0;
    int cur_[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // one cursor per page (0-7)
};

// X04: where a window pixel falls in the picture. dest = the rectangle (in window pixels) the content_w x content_h image is drawn into (display::Layout::dest).
// Returns false when the pointer is outside it. The picture (menu) is the left kPictureW x kPictureH part of the content (the key panel is to the right of it).
inline bool window_to_picture(int dest_x, int dest_y, int dest_w, int dest_h, int content_w, int content_h, int wx, int wy, int *px, int *py) {
    if (dest_w <= 0 || dest_h <= 0 || wx < dest_x || wy < dest_y || wx >= dest_x + dest_w || wy >= dest_y + dest_h) return false;
    *px = (int)((long long)(wx - dest_x) * content_w / dest_w);
    *py = (int)((long long)(wy - dest_y) * content_h / dest_h);
    return true;
}

// X05: the Bindings screen and the key panel under the mouse (picture coordinates, 352 x 288). bindings_cell_at: the slot (row 0..11, column 0 KEY 1, 1 KEY 2, 2 PAD BUTTON)
// under the pointer; a click on a command's name goes to its first slot that can change. help_pin_at: the header of the key panel (clicking it pins / unpins the panel).
bool bindings_cell_at(int px, int py, int *row, int *col);
bool help_pin_at(int px, int py);

// S34A: names for the screen (game font: capital letters, digits, dash) and the bindings screen itself (across the whole 352-pixel width).
std::string display_key(int scancode); // "UP", "LEFT SHIFT", "KEYPAD 5", "COMMA" ... ("-" for none)
std::string display_pad(int button);   // "BACK", "START", "A", "LB" ... ("-" for none)
void binding_lines(const controls::Bindings &b, std::string out[7]);
void binding_keycaps(const controls::Bindings &b, std::string out[7][2]); // X08: for Model::key_cap      // for Model::bind_line
void pad_names(const controls::Bindings &b, std::string out[5]);          // for Model::pad_name
void pad_buttons(const controls::Bindings &b, int out[5]);                // for Model::pad_btn
void draw_bindings(const Gfx &g, uint32_t *out, const controls::BindEditor &e, bool pad_connected, PadType type = PadType::Other);
// S34C: button pictures drawn in code. The picture (or, for buttons without one, a small labelled box) of SDL button `button` (or controls::kPadLeftTrigger / kPadRightTrigger)
// on a pad of this type, at (x, y), at most maxw pixels wide (plain clipped text if it does not fit). Returns the width drawn. glyph_label() is the word for the button ("" for the
// four face buttons, which are drawn as shapes, and for unknown pads it is the plain name).
int draw_glyph(const Gfx &g, uint32_t *out, int x, int y, PadType t, int button, int maxw);
int glyph_width(PadType t, int button);
std::string glyph_label(PadType t, int button);
void draw_pad_icon(uint32_t *out, int x, int y, PadType t);              // a small controller picture, 16 x 10 pixels
void draw_toast(const Gfx &g, uint32_t *out, PadType t, bool connected); // "PS4 CONNECTED" / "PS4 REMOVED" with the icon, at the top of the picture
void draw_help(const Gfx &g, uint32_t *out, const Model &m);
void draw_focus_pause(const Gfx &g, uint32_t *out);
void draw_achievement_popup(const Gfx &g, uint32_t *out, int id);                                    // X25: the unlock pop-up at the top of the picture
void draw_new_best(const Gfx &g, uint32_t *out, const std::string &line1, const std::string &line2); // X24: the banner shown when a level is cleared with a record
void draw_run_summary(const Gfx &g, uint32_t *out, const stats::Summary &s);                         // X22: the game-over statistics page (224x288 picture)
// X08: a key cap drawn in code (a box around the key name; the four arrows are a small triangle). Returns the width drawn; the cap is 10 pixels high, its text row at y.
int keycap_width(const std::string &label);
int draw_keycap(const Gfx &g, uint32_t *out, int x, int y, const std::string &label);
// X08: true when the hints (help panel, menu footer) show the pad's pictures: Hints = PAD, or AUTO and the last keyboard / pad use was the pad, and a pad is connected.
bool hints_are_pad(
    const Model &m); // X06: dims the picture and says PAUSED / PRESS ANY KEY (the window lost focus)           // the side panel (columns 224..351) with every key
} // namespace menu
