// S35: the first-run ROM finder with a pretend user (no window). Usage: t_firstrun DIR ROMZIP   (DIR holds the made-up files written by tests/test_firstrun.sh)
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "../src/app/firstrun.h"
static int n = 0, fails = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
struct Fake { // a pretend player: answers the questions from a script and remembers what was shown
    std::vector<bool> yes;
    std::vector<std::string> picks;
    std::vector<std::string> shown, logs;
    size_t yi = 0, pi = 0;
    firstrun::Ui ui() {
        firstrun::Ui u;
        u.ask = [this](const std::string &, const std::string &t, const std::string &) {
            shown.push_back(t);
            return yi < yes.size() ? (bool)yes[yi++] : false;
        };
        u.pick = [this](std::string *o) {
            if (pi >= picks.size() || picks[pi].empty()) {
                ++pi;
                return false;
            }
            *o = picks[pi++];
            return true;
        };
        return u;
    }
};
static bool has(const std::string &s, const char *t) { return s.find(t) != std::string::npos; }
int main(int argc, char **argv) {
    const std::string D = argc > 1 ? argv[1] : ".", ROM = argc > 2 ? argv[2] : "roms/pacman.zip";
    std::string rom;
    auto nolog = [](const std::string &) {};
    // 1. a good ROM next to the program: no question asked
    {
        Fake f;
        firstrun::Ui u = f.ui();
        rom.clear();
        CHECK(firstrun::resolve(rom, {ROM}, D + "/cfg1", &u, nolog) && rom == ROM && f.shown.empty(), "ROM found in the first place: accepted, nothing asked");
    }
    // 2. nothing anywhere, and no dialogs (scripted run): false, and the message goes to the log
    {
        std::vector<std::string> logs;
        rom.clear();
        CHECK(!firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, D + "/cfg2", nullptr, [&](const std::string &t) { logs.push_back(t); }) && logs.size() == 1 &&
                  has(logs[0], "pacman.zip") && has(logs[0], "not found"),
              "no ROM, no dialogs: refused with the friendly message in the log");
    }
    // 3. nothing found: message shown, the player closes it
    {
        Fake f;
        f.yes = {false};
        firstrun::Ui u = f.ui();
        rom.clear();
        CHECK(!firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, D + "/cfg3", &u, nolog) && f.shown.size() == 1, "message shown once; Close ends the program's start");
        CHECK(has(f.shown[0], "does not contain the game") && has(f.shown[0], "pacman.zip") && has(f.shown[0], "same folder") && has(f.shown[0], "Choose file"),
              "the missing-ROM message says what to do in plain words");
        CHECK(!has(f.shown[0], "CRC") && !has(f.shown[0], "checksum") && !has(f.shown[0], "exception"), "no technical words in the missing-ROM message");
    }
    // 4. nothing found, the player chooses the right file: accepted and remembered; the next start finds it without a question
    {
        Fake f;
        f.yes = {true};
        f.picks = {ROM};
        firstrun::Ui u = f.ui();
        rom.clear();
        const std::string cfg = D + "/cfg4";
        CHECK(firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, cfg, &u, nolog) && rom == ROM, "picked a good file: accepted");
        CHECK(firstrun::read_remembered(cfg) == ROM, "the chosen place is remembered");
        Fake g;
        firstrun::Ui u2 = g.ui();
        rom.clear();
        CHECK(firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, cfg, &u2, nolog) && rom == ROM && g.shown.empty(),
              "next start: found through the remembered place, nothing asked");
    }
    // 5. the remembered file has gone: asked again
    {
        const std::string cfg = D + "/cfg5";
        firstrun::write_remembered(cfg, D + "/gone.zip");
        Fake f;
        f.yes = {false};
        firstrun::Ui u = f.ui();
        rom.clear();
        CHECK(!firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, cfg, &u, nolog) && f.shown.size() == 1, "remembered file missing: the question comes again");
    }
    // 6. wrong files: each gets a plain explanation, then the player picks the right one
    struct W {
        const char *file;
        const char *must;
        const char *name;
    } wrong[] = {{"text.zip", "not a zip file", "a text file called .zip"},
                 {"empty.zip", "different game or an incomplete set", "a zip without the game's files"},
                 {"badcrc.zip", "not the original ones", "a zip with the right names but a changed file"},
                 {"cut.zip", "not a zip file", "a zip cut in half"}};
    int wi = 0;
    for (const W &w : wrong) {
        Fake f;
        f.yes = {true, true};
        f.picks = {D + "/" + w.file, ROM};
        firstrun::Ui u = f.ui();
        rom.clear();
        const bool ok = firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, D + "/cfg6_" + std::to_string(wi++), &u, nolog);
        CHECK(ok && rom == ROM && f.shown.size() == 2, (std::string("wrong file (") + w.name + "), then the right one: accepted after one explanation").c_str());
        CHECK(f.shown.size() == 2 && has(f.shown[1], "This file cannot be used") && has(f.shown[1], w.must) && has(f.shown[1], "complete, unmodified ROM set") &&
                  has(f.shown[1], "Details:"),
              (std::string("explanation for ") + w.name + " says what is wrong in plain words").c_str());
    }
    // 7. a wrong file given on the command line (or found next to the program) is explained too
    {
        Fake f;
        f.yes = {false};
        firstrun::Ui u = f.ui();
        rom = D + "/text.zip";
        CHECK(!firstrun::resolve(rom, {ROM}, D + "/cfg7", &u, nolog) && f.shown.size() == 1 && has(f.shown[0], "cannot be used"),
              "a wrong file named on the command line is explained, not silently replaced");
    }
    // 8. the player cancels the picker
    {
        Fake f;
        f.yes = {true};
        f.picks = {""};
        firstrun::Ui u = f.ui();
        rom.clear();
        CHECK(!firstrun::resolve(rom, {D + "/nothing/pacman.zip"}, D + "/cfg8", &u, nolog), "picker cancelled: start is refused politely");
    }
    // 9. the program never writes the ROM anywhere: only rom_path.txt (a path) is written
    {
        std::ifstream r(D + "/cfg4/rom_path.txt");
        std::string l;
        std::getline(r, l);
        CHECK(l == ROM && r.good(), "rom_path.txt holds only the path");
    }
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
