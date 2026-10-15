// S35: finding the ROM on the first run, with friendly messages. The ROM is never part of the program: the player's own pacman.zip is looked for next to the program
// (and in a roms folder next to it, and in the current folder), then at the place chosen last time; if none works, a message explains and a file picker lets the player choose it.
// A chosen file is remembered (rom_path.txt in the settings folder) so that the question is asked only once. Nothing here copies, writes or unpacks the ROM.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace firstrun {

constexpr const char *kAppName = "Maze Chase"; // the program's own name (the window title and the dialogs); the original game's name is not used

struct
    Ui { // what the player sees: ask() shows a message with a button for the action and a Close button (true = the action button); pick() shows a file picker (false = cancelled)
    std::function<bool(const std::string &title, const std::string &text, const std::string &action)> ask;
    std::function<bool(std::string *path)> pick;
};

std::string missing_text(const std::string &exe_dir);                                // "the ROM was not found" in plain words
std::string rejected_text(const std::string &path, const std::string &loader_error); // "this file is not the right one" in plain words, with the reason
std::string remembered_file(const std::string &settings_dir);                        // settings_dir/rom_path.txt
std::string read_remembered(const std::string &settings_dir);                        // "" if none
bool write_remembered(const std::string &settings_dir, const std::string &rom_path);

// Finds a usable ROM zip. rom: a path given on the command line (may be empty). candidates: places to look, in order. ui: null = no dialogs (the message goes to log and false is returned).
// On success rom holds the path of a zip that was read and accepted. The ROM is only read to check it.
bool resolve(std::string &rom, const std::vector<std::string> &candidates, const std::string &settings_dir, Ui *ui, const std::function<void(const std::string &)> &log);

Ui system_ui(); // the real dialogs: SDL message boxes, and on Windows the standard "Open file" window (elsewhere there is no picker: the message tells where to put the file)

} // namespace firstrun
