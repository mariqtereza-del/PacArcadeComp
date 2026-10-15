// S32: see controls.h
#include "controls.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace controls {

static const char *kActionKey[kActions] = {"up", "down", "left", "right", "coin", "start1", "start2"};
static const char *kActionLabel[kActions] = {"UP", "DOWN", "LEFT", "RIGHT", "COIN", "START 1 PLAYER", "START 2 PLAYERS"};
static const char *kPadKey[kPadActions] = {"pad_coin", "pad_start1", "pad_start2", "pad_menu", "pad_pause", "pad_save", "pad_load", "pad_rewind"};
static const char *kPadLabel[kPadActions] = {"COIN", "START 1 PLAYER", "START 2 PLAYERS", "MENU", "PAUSE", "QUICK SAVE", "QUICK LOAD", "REWIND"};

Bindings::Bindings() {
    for (auto &a : key) a[0] = a[1] = 0;
    key[Up][0] = SDL_SCANCODE_UP;
    key[Up][1] = SDL_SCANCODE_W;
    key[Down][0] = SDL_SCANCODE_DOWN;
    key[Down][1] = SDL_SCANCODE_S;
    key[Left][0] = SDL_SCANCODE_LEFT;
    key[Left][1] = SDL_SCANCODE_A;
    key[Right][0] = SDL_SCANCODE_RIGHT;
    key[Right][1] = SDL_SCANCODE_D;
    key[Coin][0] = SDL_SCANCODE_5;
    key[Coin][1] = SDL_SCANCODE_KP_5;
    key[Start1][0] = SDL_SCANCODE_1;
    key[Start1][1] = SDL_SCANCODE_KP_1;
    key[Start2][0] = SDL_SCANCODE_2;
    key[Start2][1] = SDL_SCANCODE_KP_2;
    for (int &p : pad) p = -1;
    pad[PadCoin] = SDL_CONTROLLER_BUTTON_BACK;
    pad[PadStart1] = SDL_CONTROLLER_BUTTON_START;
    pad[PadMenu] = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    one_button_start = true;
}
bool Bindings::operator==(const Bindings &o) const {
    for (int a = 0; a < kActions; ++a)
        for (int i = 0; i < 2; ++i)
            if (key[a][i] != o.key[a][i]) return false;
    for (int a = 0; a < kPadActions; ++a)
        if (pad[a] != o.pad[a]) return false;
    return one_button_start == o.one_button_start;
}

const char *action_label(Action a) { return kActionLabel[a]; }
const char *pad_action_label(PadAction a) { return kPadLabel[a]; }

static std::string lower(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
static std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string key_name(int sc) {
    if (sc <= 0) return "none";
    const char *n = SDL_GetScancodeName((SDL_Scancode)sc);
    return (n && *n) ? n : "none";
}
int key_from_name(const std::string &name) {
    std::string n = trim(name);
    if (n.empty() || lower(n) == "none") return 0;
    return (int)SDL_GetScancodeFromName(n.c_str()); // SDL_SCANCODE_UNKNOWN (0) when not a key
}
std::string pad_name(int b) {
    if (b < 0) return "none";
    if (b == kPadLeftTrigger) return "lefttrigger";
    if (b == kPadRightTrigger) return "righttrigger";
    const char *n = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)b);
    return n ? n : "none";
}
int pad_from_name(const std::string &name, bool *ok) {
    std::string n = lower(trim(name));
    if (ok) *ok = true;
    if (n.empty() || n == "none") return -1;
    if (n == "lefttrigger") return kPadLeftTrigger;
    if (n == "righttrigger") return kPadRightTrigger;
    SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(n.c_str());
    if (b == SDL_CONTROLLER_BUTTON_INVALID) {
        if (ok) *ok = false;
        return -1;
    }
    return (int)b;
}

std::string default_config_path() {
    namespace fs = std::filesystem;
    const char *ad = std::getenv("APPDATA");
    if (ad && *ad) return (fs::path(ad) / "PacmanPort" / "controls.cfg").string();
    const char *xdg = std::getenv("XDG_CONFIG_HOME"), *home = std::getenv("HOME");
    if (xdg && *xdg) return (fs::path(xdg) / "pacman-port" / "controls.cfg").string();
    if (home && *home) return (fs::path(home) / ".config" / "pacman-port" / "controls.cfg").string();
    return "";
}

std::string to_text(const Bindings &b) {
    std::ostringstream o;
    o << "# Pac-Man port controls. You can edit this file with Notepad, or change the controls in the game (F9 keys, F10 pad, F12 reset).\n"
         "# Keys are named as in SDL (Up, W, Space, Keypad 5, Left Shift ...). \"none\" = not used.\n";
    for (int a = 0; a < kActions; ++a) o << kActionKey[a] << " = " << key_name(b.key[a][0]) << ", " << key_name(b.key[a][1]) << "\n";
    for (int a = 0; a < kPadActions; ++a) o << kPadKey[a] << " = " << pad_name(b.pad[a]) << "\n";
    o << "one_button_start = " << (b.one_button_start ? "on" : "off") << "\n";
    return o.str();
}

Bindings parse(const std::string &text, std::string *problems) {
    Bindings b;
    std::ostringstream pr;
    std::istringstream in(text);
    std::string line;
    int ln = 0;
    while (std::getline(in, line)) {
        ++ln;
        size_t h = line.find('#');
        if (h != std::string::npos) line.erase(h);
        line = trim(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            pr << "line " << ln << ": no '=' found\n";
            continue;
        }
        std::string k = lower(trim(line.substr(0, eq))), v = trim(line.substr(eq + 1));
        bool done = false;
        for (int a = 0; a < kActions && !done; ++a)
            if (k == kActionKey[a]) {
                done = true;
                std::string parts[2];
                size_t c = v.find(',');
                parts[0] = trim(c == std::string::npos ? v : v.substr(0, c));
                parts[1] = c == std::string::npos ? "none" : trim(v.substr(c + 1));
                for (int i = 0; i < 2; ++i) {
                    int sc = key_from_name(parts[i]);
                    if (sc == 0 && lower(parts[i]) != "none" && !parts[i].empty()) {
                        pr << "line " << ln << ": unknown key '" << parts[i] << "'\n";
                        sc = b.key[a][i];
                    } else if (key_is_reserved(sc)) {
                        pr << "line " << ln << ": key '" << parts[i] << "' is reserved (Esc and F keys)\n";
                        sc = b.key[a][i];
                    }
                    b.key[a][i] = sc;
                }
            }
        for (int a = 0; a < kPadActions && !done; ++a)
            if (k == kPadKey[a]) {
                done = true;
                bool ok;
                int bt = pad_from_name(v, &ok);
                if (!ok) pr << "line " << ln << ": unknown pad button '" << v << "'\n";
                else b.pad[a] = bt;
            }
        if (!done && k == "one_button_start") {
            done = true;
            std::string lv = lower(v);
            if (lv == "on" || lv == "yes" || lv == "1" || lv == "true") b.one_button_start = true;
            else if (lv == "off" || lv == "no" || lv == "0" || lv == "false") b.one_button_start = false;
            else pr << "line " << ln << ": one_button_start must be on or off\n";
        }
        if (!done) pr << "line " << ln << ": unknown setting '" << k << "'\n";
    }
    if (problems) *problems = pr.str();
    return b;
}

bool load(const std::string &path, Bindings &b, std::string *problems) {
    b = Bindings();
    if (problems) problems->clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    b = parse(ss.str(), problems);
    return true;
}

bool save(const std::string &path, const Bindings &b, std::string *error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (path.empty()) {
        if (error) *error = "no settings folder found";
        return false;
    }
    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (error) *error = "cannot write " + tmp;
            return false;
        }
        f << to_text(b);
        if (!f) {
            if (error) *error = "write failed " + tmp;
            return false;
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
    } // older Windows: rename onto an existing file fails
    if (ec) {
        if (error) *error = "cannot replace " + path + ": " + ec.message();
        return false;
    }
    return true;
}

bool key_is_reserved(int sc) {
    return sc == SDL_SCANCODE_ESCAPE || (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) || (sc >= SDL_SCANCODE_F13 && sc <= SDL_SCANCODE_F24);
}

// ---- StartAssist. phases: 0 idle, 1 coin held, 2 waiting for the credit, 3 start held. A press while the ROM is still starting up (main state 0,
// about 5 seconds) is remembered for up to 10 seconds and acted on when the attract screen appears.
void StartAssist::update(bool start_held, int credits, int main_state, bool *coin, bool *start) {
    *coin = *start = false;
    bool edge = start_held && !was_held_;
    was_held_ = start_held;
    if (phase_ != 0 && (main_state == 3 || (main_state == 2 && credits == 0))) {
        phase_ = 0;
        pending_ = 0;
        return;
    } // the game has started by itself
    if (pending_ > 0) --pending_;
    switch (phase_) {
    case 0:
        if (edge && credits == 0 && (main_state == 0 || main_state == 1)) pending_ = 600;
        if (pending_ > 0 && credits == 0 && main_state == 1) {
            pending_ = 0;
            phase_ = 1;
            left_ = 5;
            coins_ = 1;
            waited_ = 0;
            *coin = true;
            --left_;
        }
        break;
    case 1:
        *coin = true;
        if (--left_ <= 0) {
            phase_ = 2;
            waited_ = 0;
        }
        break;
    case 2:
        ++waited_;
        if (credits > 0 && waited_ >= 3) {
            phase_ = 3;
            left_ = 4;
            *start = true;
            --left_;
        } else if (waited_ >= 60 && coins_ < 2 && credits == 0) {
            ++coins_;
            phase_ = 1;
            left_ = 5;
            *coin = true;
            --left_;
        } // a coin the ROM did not take
        else if (waited_ >= 120) {
            phase_ = 3;
            left_ = 4;
            *start = true;
            --left_;
        } // free play: no credit is ever shown
        break;
    case 3:
        *start = true;
        if (--left_ <= 0) phase_ = 0;
        break;
    }
}

// ---- Remap
void Remap::begin(Kind k, const Bindings &cur) {
    kind_ = k;
    step_ = 0;
    cancelled_ = false;
    b_ = cur;
}
std::string Remap::prompt() const {
    if (kind_ == Keys) return std::string("Press the key for ") + kActionLabel[step_] + " (Esc cancels)";
    if (kind_ == Pad) return std::string("Press the pad button for ") + kPadLabel[step_] + " (Backspace = none, Esc cancels)";
    return "";
}
bool Remap::on_key(int sc, std::string *msg) {
    if (!active()) return false;
    if (sc == SDL_SCANCODE_ESCAPE) {
        cancelled_ = true;
        kind_ = None;
        if (msg) *msg = "Remap cancelled, nothing changed";
        return true;
    }
    if (kind_ == Pad) {
        if (sc == SDL_SCANCODE_BACKSPACE) {
            b_.pad[step_] = -1;
            if (msg) *msg = std::string(kPadLabel[step_]) + " = none";
            if (++step_ >= kLegacyPadActions) {
                kind_ = None;
                return true;
            }
        } else if (msg) *msg = "That is a keyboard key; press a button on the pad";
        return false;
    }
    if (key_is_reserved(sc)) {
        if (msg) *msg = "That key is reserved (Esc and the F keys); press another key";
        return false;
    }
    for (int a = 0; a < kActions; ++a)
        for (int i = 0; i < 2; ++i)
            if (b_.key[a][i] == sc) b_.key[a][i] = 0; // one key, one control
    b_.key[step_][0] = sc;
    b_.key[step_][1] = 0;
    if (msg) *msg = std::string(kActionLabel[step_]) + " = " + key_name(sc);
    if (++step_ >= kActions) {
        kind_ = None;
        return true;
    }
    return false;
}
bool Remap::on_pad_button(int button, std::string *msg) {
    if (kind_ != Pad) return false;
    for (int a = 0; a < kPadActions; ++a)
        if (b_.pad[a] == button) b_.pad[a] = -1;
    b_.pad[step_] = button;
    if (msg) *msg = std::string(kPadLabel[step_]) + " = " + pad_name(button);
    if (++step_ >= kLegacyPadActions) {
        kind_ = None;
        return true;
    }
    return false;
}

// ---- S34A: BindEditor
static std::string upper(std::string s) {
    for (auto &c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
bool pad_button_is_dpad(int b) {
    return b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN || b == SDL_CONTROLLER_BUTTON_DPAD_LEFT || b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
}
static const char *kRowLabel[BindEditor::kRows] = {"UP", "DOWN", "LEFT", "RIGHT", "COIN", "START 1", "START 2", "MENU", "PAUSE", "SAVE", "LOAD", "REWIND"};
const char *fixed_key_label(int row) {
    static const char *k[] = {"ESC", "F1", "F2", "F4", "BACKSPACE"};
    return (row >= kActions && row < kActions + 5) ? k[row - kActions] : "";
}
PadCommand pad_command_for(const Bindings &b, int button) {
    if (button < 0) return PadCommand::None;
    static const PadAction acts[5] = {PadMenu, PadPause, PadSave, PadLoad, PadRewind};
    static const PadCommand cmds[5] = {PadCommand::Menu, PadCommand::Pause, PadCommand::Save, PadCommand::Load, PadCommand::Rewind};
    for (int i = 0; i < 5; ++i)
        if (b.pad[acts[i]] == button) return cmds[i];
    return PadCommand::None;
}

void BindEditor::begin(const Bindings &cur, int row, int col) {
    active_ = true;
    waiting_ = false;
    b_ = cur;
    row_ = std::max(0, std::min(kRows - 1, row));
    col_ = std::max(0, std::min(kCols - 1, col));
    msg_.clear();
}
void BindEditor::move(int dr, int dc) {
    if (!active_ || waiting_) return;
    row_ = (row_ + dr + kRows) % kRows;
    col_ = (col_ + dc + kCols) % kCols;
    msg_.clear();
}
void BindEditor::point(int row, int col) {
    if (!active_ || waiting_) return;
    row_ = std::max(0, std::min(kRows - 1, row));
    col_ = std::max(0, std::min(kCols - 1, col));
}
void BindEditor::click_cell(int row, int col) {
    if (!active_) return;
    waiting_ = false;
    row_ = std::max(0, std::min(kRows - 1, row));
    col_ = std::max(0, std::min(kCols - 1, col));
    msg_.clear();
    select();
}
void BindEditor::select() {
    if (!active_ || waiting_) return;
    if (col_ == 2 && row_ < Coin) {
        msg_ = "THE D-PAD AND LEFT STICK MOVE - FIXED";
        return;
    }
    if (col_ < 2 && row_ >= kActions) {
        msg_ = std::string(kRowLabel[row_]) + " IS ALWAYS ON " + fixed_key_label(row_) + " - FIXED";
        return;
    }
    waiting_ = true;
    msg_.clear();
}
bool BindEditor::clear() {
    if (!active_) return false;
    waiting_ = false;
    if (col_ == 2) {
        if (row_ < Coin) {
            msg_ = "THE D-PAD AND LEFT STICK MOVE - FIXED";
            return false;
        }
        int &p = b_.pad[row_ - Coin];
        if (p < 0) {
            msg_ = "ALREADY EMPTY";
            return false;
        }
        p = -1;
        msg_ = std::string(kRowLabel[row_]) + " PAD BUTTON CLEARED";
        return true;
    }
    if (row_ >= kActions) {
        msg_ = std::string(kRowLabel[row_]) + " IS ALWAYS ON " + fixed_key_label(row_) + " - FIXED";
        return false;
    }
    int &k = b_.key[row_][col_];
    if (k == 0) {
        msg_ = "ALREADY EMPTY";
        return false;
    }
    k = 0;
    msg_ = std::string(kRowLabel[row_]) + " KEY " + std::to_string(col_ + 1) + " CLEARED";
    if (!b_.key[row_][0] && !b_.key[row_][1]) msg_ += " - NO KEY LEFT";
    return true;
}
bool BindEditor::on_key(int sc) {
    if (!active_ || !waiting_) return false;
    if (col_ == 2) {
        msg_ = "PRESS A BUTTON ON THE PAD";
        return false;
    }
    if (row_ >= kActions) return false;
    if (key_is_reserved(sc)) {
        msg_ = "ESC AND F KEYS ARE RESERVED";
        return false;
    }
    int &slot = b_.key[row_][col_];
    const int old = slot;
    waiting_ = false;
    if (old == sc) {
        msg_ = "ALREADY SET";
        return false;
    }
    std::string nm = upper(key_name(sc));
    for (int a = 0; a < kActions; ++a)
        for (int i = 0; i < 2; ++i) {
            if ((a == row_ && i == col_) || b_.key[a][i] != sc) continue;
            b_.key[a][i] = old;
            slot = sc;
            if (a == row_) msg_ = nm + " WAS ON THE OTHER SLOT - SLOTS SWAPPED";
            else msg_ = nm + " WAS ON " + kRowLabel[a] + " - SWAPPED" + (old ? "" : " - " + std::string(kRowLabel[a]) + " NOW EMPTY");
            return true;
        }
    slot = sc;
    msg_ = std::string(kRowLabel[row_]) + " = " + nm;
    return true;
}
bool BindEditor::on_pad_button(int button) {
    if (!active_ || !waiting_ || col_ != 2) return false;
    if (!pad_button_exists(button)) return false;
    if (pad_button_is_dpad(button)) {
        msg_ = "D-PAD MOVES PAC-MAN - TRY ANOTHER";
        return false;
    }
    int &slot = b_.pad[row_ - Coin];
    const int old = slot;
    waiting_ = false;
    if (old == button) {
        msg_ = "ALREADY SET";
        return false;
    }
    std::string nm = upper(pad_name(button));
    for (int a = 0; a < kPadActions; ++a) {
        if (a == row_ - Coin || b_.pad[a] != button) continue;
        b_.pad[a] = old;
        slot = button;
        msg_ = nm + " WAS ON " + kRowLabel[Coin + a] + " - SWAPPED";
        return true;
    }
    slot = button;
    msg_ = std::string(kRowLabel[row_]) + " = " + nm;
    return true;
}
bool BindEditor::back() {
    if (!active_) return false;
    if (waiting_) {
        waiting_ = false;
        msg_ = "CANCELLED - NOTHING CHANGED";
        return false;
    }
    active_ = false;
    return true;
}
std::string BindEditor::prompt() const {
    if (!active_) return "";
    if (waiting_)
        return col_ == 2 ? std::string("PRESS A BUTTON FOR ") + kRowLabel[row_] + " - ESC CANCELS" : std::string("PRESS A KEY FOR ") + kRowLabel[row_] + " - ESC CANCELS";
    return "ENTER CHANGES  BKSP CLEARS  ESC BACK";
}

bool pad_button_exists(int b) { return (b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX) || b == kPadLeftTrigger || b == kPadRightTrigger; }
void TriggerButtons::update(int left, int right, std::vector<Change> &out) {
    auto step = [&](bool &st, int v, int id) {
        if (!st && v > 16384) {
            st = true;
            out.push_back({id, true});
        } else if (st && v < 8192) {
            st = false;
            out.push_back({id, false});
        }
    };
    step(l_, left, kPadLeftTrigger);
    step(r_, right, kPadRightTrigger);
}

bool PadAnnouncer::should_announce(const std::string &name, long now_ms) {
    if (name == last_ && now_ms - at_ < 2000) return false;
    last_ = name;
    at_ = now_ms;
    return true;
}

} // namespace controls
