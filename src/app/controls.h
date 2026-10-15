// S32: the player's controls: which keys and pad buttons mean what, the settings file, the one-button Start, and the remap dialogue.
// None of this can touch the emulation: it only decides which of the logical controls (src/board/inputs.h) are pressed. The emulated input bits,
// the ROM and the timing are not changed. The settings file lives in the user's AppData folder (Windows: %APPDATA%\PacmanPort\controls.cfg;
// elsewhere: $XDG_CONFIG_HOME/pacman-port/controls.cfg or ~/.config/pacman-port/controls.cfg).
//
// File format (plain text, one setting per line, # starts a comment; the program rewrites the file whenever something is changed in the game):
//   up = Up, W          up to two keys per control, named as in SDL (Up, W, Space, Keypad 5, Left Shift ...), or "none"
//   coin = 5, Keypad 5
//   pad_coin = back     (also pad_start1, pad_start2, and since S34B pad_menu, pad_pause, pad_save, pad_load, pad_rewind) pad buttons are named as in SDL (a b x y back start leftshoulder ...), or "none"
//   one_button_start = on
#pragma once
#include <string>
#include <vector>

namespace controls {

enum Action { Up, Down, Left, Right, Coin, Start1, Start2, kActions };
enum PadAction {
    PadCoin,
    PadStart1,
    PadStart2,
    PadMenu,
    PadPause,
    PadSave,
    PadLoad,
    PadRewind, // S34B: the system commands that have a keyboard key (Esc, F1, F2, F4, Backspace)
    kPadActions
};
const int kLegacyPadActions = 3;
// The analog triggers (L2/R2 on a PlayStation pad, LT/RT on an Xbox pad) are AXES in SDL, not buttons. They are given button numbers of their own so they can be
// bound like any button: SDL's buttons are 0..20, these are 100 and 101. TriggerButtons turns the two axis values into press/release changes.
const int kPadLeftTrigger = 100, kPadRightTrigger = 101; // the S32 dialogue (Remap) only asks for coin, start 1 and start 2

struct Bindings {
    int key[kActions][2];  // SDL scancodes, 0 = none
    int pad[kPadActions];  // SDL game controller buttons, -1 = none
    bool one_button_start; // pressing Start with no credit and no game inserts the coin first
    Bindings();            // the defaults: arrows/WASD, 5 coin, 1 and 2 start; pad Back = coin, Start = 1 player, right stick click = menu; one-button Start on
    bool operator==(const Bindings &o) const;
};

const char *action_label(Action a); // "UP", "COIN" ... for messages
const char *pad_action_label(PadAction a);
std::string key_name(int scancode);         // "Left", "5" ..., "none" for 0
int key_from_name(const std::string &name); // 0 when unknown
std::string pad_name(int button);           // "back", "start" ..., "none" for -1
int pad_from_name(const std::string &name, bool *ok = nullptr);

std::string default_config_path(); // empty if no home folder can be found
// Text of the file for these bindings, and the reverse. parse() keeps the defaults for anything missing and puts a line for every
// problem in `problems` (unknown key name, unknown setting); it never fails as a whole.
std::string to_text(const Bindings &b);
Bindings parse(const std::string &text, std::string *problems = nullptr);
bool load(const std::string &path, Bindings &b, std::string *problems);    // false if the file does not exist (b = defaults)
bool save(const std::string &path, const Bindings &b, std::string *error); // writes a temporary file, then renames it over the real one

// A key that may not be given to a control: Esc (closes the program) and the function keys (display and remap keys).
bool key_is_reserved(int scancode);

// ---- one-button Start. Call once per frame with the raw "Start 1 is held" and what the machine shows; it says when to add a coin press
// and a Start press on top of the player's own presses. It only ever acts in the attract screen with no credit (main state 1, credits 0).
class StartAssist {
public:
    void update(bool start_held, int credits, int main_state, bool *coin, bool *start);
    bool busy() const { return phase_ != 0; }

private:
    int phase_ = 0, left_ = 0, waited_ = 0, coins_ = 0, pending_ = 0;
    bool was_held_ = false;
};

// ---- the remap dialogue (F9 keys, F10 pad). It works on a copy; nothing changes until every step is answered.
class Remap {
public:
    enum Kind { None, Keys, Pad };
    bool active() const { return kind_ != None; }
    Kind kind() const { return kind_; }
    void begin(Kind k, const Bindings &current);
    std::string prompt() const; // what to show the player now
    // Feed one key press / pad button press. Returns true when the dialogue has finished (result() holds the new bindings) or was cancelled.
    bool on_key(int scancode, std::string *message);
    bool on_pad_button(int button, std::string *message);
    bool cancelled() const { return cancelled_; }
    const Bindings &result() const { return b_; }

private:
    Kind kind_ = None;
    int step_ = 0;
    bool cancelled_ = false;
    Bindings b_;
};

// ---- S34A: the bindings screen's logic. A table of 7 commands (rows) by 3 slots (columns: key 1, key 2, pad button). The pad column of the four
// movement commands is fixed (d-pad and left stick) and cannot be changed. Everything works on a copy; the caller saves bindings() whenever a call returns true.
// One key (or one pad button) belongs to one command: giving a key that is already used swaps the two slots and says so in message().
class BindEditor {
public:
    static const int kRows = kActions + 5, kCols = 3;    // 7 game commands, then MENU, PAUSE, SAVE, LOAD, REWIND (pad button editable, keyboard key fixed)
    static int pad_index(int row) { return row - Coin; } // rows Coin.. map to the pad table
    bool active() const { return active_; }
    bool waiting() const { return waiting_; }
    int row() const { return row_; }
    int col() const { return col_; }
    const Bindings &bindings() const { return b_; }
    const std::string &message() const { return msg_; }
    void begin(const Bindings &current, int row = 0, int col = 0);
    void move(int drow, int dcol);     // browsing only (while waiting for a key nothing moves)
    void point(int row, int col);      // X05: the mouse is over this slot (browsing only: while waiting for a key nothing moves)
    void click_cell(int row, int col); // X05: a click: go to this slot and wait for the new key / pad button (a fixed slot says why it cannot change)
    void select();                     // Enter: wait for the new key / pad button for the selected slot
    bool clear();                      // Backspace: empty the selected slot (also ends waiting); true if the bindings changed
    bool on_key(int scancode);         // while waiting for a key; true if the bindings changed
    bool on_pad_button(int button);    // while waiting for a pad button; true if the bindings changed
    bool back();                       // Esc: cancels waiting, or (when browsing) closes the screen; true when the screen was closed
    std::string prompt() const;        // one line telling the player what to do now (capital letters)
private:
    bool active_ = false, waiting_ = false;
    int row_ = 0, col_ = 0;
    Bindings b_;
    std::string msg_;
};

class TriggerButtons {
public:
    struct Change {
        int button;
        bool down;
    };
    // axis values as SDL gives them (0..32767; some pads report negative at rest: treated as 0). Pressed above 50 %, released below 25 % (so a trigger resting half way does not flutter).
    void update(int left, int right, std::vector<Change> &out);
    void reset() { l_ = r_ = false; }
    bool left_down() const { return l_; }
    bool right_down() const { return r_; }

private:
    bool l_ = false, r_ = false;
};
bool pad_button_is_dpad(int button); // the four d-pad buttons: they move Pac-Man and cannot be bound
bool pad_button_exists(int button);  // an SDL button (0..20) or one of the two triggers

// ---- S34B: what a pad button does while the game is running
enum class PadCommand { None, Menu, Pause, Save, Load, Rewind };
PadCommand pad_command_for(const Bindings &b, int button); // the system command bound to this button (None for coin/start/unbound buttons)
const char *fixed_key_label(int row);                      // "ESC", "F1", "F2", "F4", "BACKSPACE" for the system rows, "" otherwise

// ---- the controller announcement: the same pad name is announced only once within `window_ms` (SDL reports a pad that is already
// plugged in when the program starts as "added" again, which showed as a second "Gamepad" line in S03).
class PadAnnouncer {
public:
    bool should_announce(const std::string &name, long now_ms);
    void forget() { last_.clear(); }

private:
    std::string last_;
    long at_ = 0;
};

} // namespace controls
