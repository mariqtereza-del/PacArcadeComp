// firstrun.cpp: finding the player's own ROM zip on the first run (see firstrun.h). The ROM is only read, never copied.
#include "firstrun.h"
#include <cstdio>
#include <filesystem>
#include <SDL.h>
#include "../board/romset.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace firstrun {

static bool exists(const std::string &p) {
    if (p.empty()) return false;
    FILE *f = std::fopen(p.c_str(), "rb");
    if (f) std::fclose(f);
    return f != nullptr;
}
static bool contains(const std::string &s, const char *t) { return s.find(t) != std::string::npos; }

std::string missing_text(const std::string &exe_dir) {
    return std::string("The ROM file was not found.\n\nThis program does not contain the game itself. You need your own copy of the game's ROM set: a zip file named "
                       "pacman.zip.\n\n") +
           "Put pacman.zip in the same folder as this program" + (exe_dir.empty() ? "" : " (" + exe_dir + ")") +
           " and start the program again.\nOr click \"Choose file\" to pick it now; the program will remember your choice.";
}
std::string rejected_text(const std::string &path, const std::string &err) {
    std::string why;
    if (contains(err, "not a zip")) why = "It is not a zip file, or it is damaged.";
    else if (contains(err, "Cannot open")) why = "The file could not be opened. It may have been moved or deleted.";
    else if (contains(err, "far too large")) why = "It is far too large to be the ROM zip.";
    else if (contains(err, "is missing from the zip"))
        why = "It is a zip file, but some of the game's files are missing from it. It looks like a different game or an incomplete set.";
    else
        why = "Some of the files inside are not the original ones (wrong size or checksum). It may be a modified or different version of the ROM set, or the zip is "
              "damaged.";
    return "This file cannot be used:\n" + path + "\n\n" + why +
           "\n\nThe program needs the complete, unmodified ROM set of the game, as one zip file named pacman.zip.\n\nDetails: " + err;
}
std::string remembered_file(const std::string &dir) { return dir.empty() ? std::string() : dir + "/rom_path.txt"; }
std::string read_remembered(const std::string &dir) {
    const std::string f = remembered_file(dir);
    if (f.empty()) return "";
    FILE *h = std::fopen(f.c_str(), "rb");
    if (!h) return "";
    char b[4096];
    size_t n = std::fread(b, 1, sizeof b - 1, h);
    std::fclose(h);
    b[n] = 0;
    std::string s(b);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}
bool write_remembered(const std::string &dir, const std::string &rom) {
    const std::string f = remembered_file(dir);
    if (f.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec); // the settings folder may not exist yet on the very first run
    FILE *h = std::fopen(f.c_str(), "wb");
    if (!h) return false;
    std::fputs(rom.c_str(), h);
    std::fputc('\n', h);
    std::fclose(h);
    return true;
}

bool resolve(std::string &rom, const std::vector<std::string> &candidates, const std::string &settings_dir, Ui *ui, const std::function<void(const std::string &)> &log) {
    std::string path = rom;
    bool picked = false;
    if (path.empty())
        for (const std::string &c : candidates)
            if (exists(c)) {
                path = c;
                break;
            }
    if (path.empty()) {
        const std::string r = read_remembered(settings_dir);
        if (exists(r)) path = r;
    }
    const std::string exe_dir =
        candidates.empty() ? std::string() : candidates[0].substr(0, candidates[0].find_last_of("/\\") == std::string::npos ? 0 : candidates[0].find_last_of("/\\"));
    for (int round = 0; round < 50; ++round) {
        std::string text, action;
        if (path.empty()) {
            text = missing_text(exe_dir);
            action = "Choose file";
        } else {
            RomSet rs;
            std::string err;
            if (load_romset_from_file(path, rs, err)) {
                rom = path;
                if (picked) write_remembered(settings_dir, path);
                return true;
            }
            text = rejected_text(path, err);
            action = "Choose another file";
        }
        if (!ui || !ui->ask || !ui->ask(std::string(kAppName) + " - ROM needed", text, action)) {
            log(text);
            return false;
        }
        std::string chosen;
        if (!ui->pick || !ui->pick(&chosen) || chosen.empty()) {
            log(text);
            return false;
        }
        path = chosen;
        picked = true;
    }
    return false;
}

Ui system_ui() {
    Ui u;
    u.ask = [](const std::string &title, const std::string &text, const std::string &action) {
        SDL_MessageBoxButtonData b[2] = {{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, action.c_str()}, {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Close"}};
        SDL_MessageBoxData d = {SDL_MESSAGEBOX_WARNING, nullptr, title.c_str(), text.c_str(), 2, b, nullptr};
        int hit = 0;
        if (SDL_ShowMessageBox(&d, &hit) < 0) { return false; }
        return hit == 1;
    };
    u.pick = [](std::string *out) {
#ifdef _WIN32
        char file[MAX_PATH] = "";
        OPENFILENAMEA o;
        ZeroMemory(&o, sizeof o);
        o.lStructSize = sizeof o;
        o.lpstrFilter = "ROM zip (*.zip)\0*.zip\0All files\0*.*\0";
        o.lpstrFile = file;
        o.nMaxFile = MAX_PATH;
        o.lpstrTitle = "Choose the ROM zip (pacman.zip)";
        o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameA(&o)) { return false; }
        *out = file;
        return true;
#else
        (void)out;
        return false; // no file picker on this system: the message says where to put the file
#endif
    };
    return u;
}

} // namespace firstrun
