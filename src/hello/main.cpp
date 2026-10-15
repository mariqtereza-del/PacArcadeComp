// Phase 2 toolchain proof: shows that a program built in the workspace runs on a real Windows PC.
// It opens a 224x288 picture (the arcade's exact size), draws a test pattern, plays a beep,
// moves a square with the keyboard or a gamepad, and writes a small report file when closed.
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

static const int W = 224, H = 288;
static const double TICK_HZ = 60.606060; // the arcade board's frame rate
static const int AUDIO_RATE = 44100;
static uint32_t fb[W * H];

static inline uint32_t RGB(int r, int g, int b) { return 0xFF000000u | (r << 16) | (g << 8) | b; }
static const uint32_t BLACK = RGB(0, 0, 16), WHITE = RGB(255, 255, 255), YELLOW = RGB(255, 255, 0), MAZEBLUE = RGB(33, 33, 222), RED = RGB(255, 0, 0),
                      PINK = RGB(255, 184, 255), CYAN = RGB(0, 255, 255), ORANGE = RGB(255, 184, 81);

static void px(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = c;
}
static void rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) px(x + i, y + j, c);
}

// Tiny 3x5 pixel font, just enough letters for the on-screen readout.
struct Glyph {
    char ch;
    const char *rows;
}; // 5 rows of 3 chars
static const Glyph FONT[] = {
    {'0', "111101101101111"}, {'1', "010110010010111"}, {'2', "111001111100111"}, {'3', "111001111001111"}, {'4', "101101111001001"}, {'5', "111100111001111"},
    {'6', "111100111101111"}, {'7', "111001001001001"}, {'8', "111101111101111"}, {'9', "111101111001111"}, {'A', "111101111101101"}, {'C', "111100100100111"},
    {'D', "110101101101110"}, {'F', "111100111100100"}, {'H', "101101111101101"}, {'I', "111010010010111"}, {'K', "101101110101101"}, {'N', "110101101101101"},
    {'O', "111101101101111"}, {'P', "111101111100100"}, {'S', "111100111001111"}, {'T', "111010010010010"}, {'X', "101101010101101"}, {'Y', "101101010010010"},
    {'Z', "111001010100111"}, {':', "000010000010000"}, {'-', "000000111000000"},
};
static void drawText(int x, int y, const std::string &s, uint32_t c, int scale = 2) {
    for (char ch : s) {
        for (const Glyph &g : FONT)
            if (g.ch == ch)
                for (int r = 0; r < 5; r++)
                    for (int k = 0; k < 3; k++)
                        if (g.rows[r * 3 + k] == '1') rect(x + k * scale, y + r * scale, scale, scale, c);
        x += 4 * scale;
    }
}

// ---- audio: a simple square-wave beep ----
static volatile int beepLeft = 0;
static volatile double beepFreq = 880.0;
static double beepPhase = 0.0;
static void audioCb(void *, Uint8 *stream, int len) {
    Sint16 *o = (Sint16 *)stream;
    int n = len / 2;
    for (int i = 0; i < n; i++) {
        if (beepLeft > 0) {
            beepPhase += beepFreq / AUDIO_RATE;
            if (beepPhase >= 1.0) beepPhase -= 1.0;
            double env = beepLeft < 400 ? beepLeft / 400.0 : 1.0; // short fade-out avoids a click
            o[i] = (Sint16)((beepPhase < 0.5 ? 1.0 : -1.0) * 0.15 * env * 32767);
            beepLeft--;
        } else o[i] = 0;
    }
}

// ---- test pattern ----
static void drawPattern(int sx, int sy, double fps, double tickRate, int refreshHz, bool padOk) {
    for (int i = 0; i < W * H; i++) fb[i] = BLACK;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) // 8px tile grid, stronger every 32px
            if (x % 8 == 0 || y % 8 == 0) fb[y * W + x] = (x % 32 == 0 || y % 32 == 0) ? RGB(40, 40, 90) : RGB(16, 16, 44);
    for (int x = 0; x < W; x++) {
        px(x, 0, WHITE);
        px(x, H - 1, WHITE);
    } // edge lines: all four must be visible
    for (int y = 0; y < H; y++) {
        px(0, y, WHITE);
        px(W - 1, y, WHITE);
    }
    for (int x = 8; x < W - 8; x++) {
        px(x, 8, MAZEBLUE);
        px(x, H - 9, MAZEBLUE);
    }
    for (int y = 8; y < H - 8; y++) {
        px(8, y, MAZEBLUE);
        px(W - 9, y, MAZEBLUE);
    }
    rect(1, 1, 7, 7, RED);
    rect(W - 8, 1, 7, 7, PINK);
    rect(1, H - 8, 7, 7, CYAN);
    rect(W - 8, H - 8, 7, 7, ORANGE);
    const uint32_t bars[7] = {YELLOW, RED, PINK, CYAN, ORANGE, MAZEBLUE, WHITE};
    for (int i = 0; i < 7; i++) rect(16 + i * 28, 16, 28, 16, bars[i]); // colour bars
    for (int x = 0; x < 192; x++) {
        int v = x * 255 / 191;
        rect(16 + x, 36, 1, 8, RGB(v, v, v));
    }                            // grey ramp
    const int cx = 112, cy = 92; // round circle = correct aspect ratio
    for (int y = -40; y <= 40; y++)
        for (int x = -40; x <= 40; x++) {
            double d = std::sqrt((double)(x * x + y * y));
            if (d > 38.5 && d <= 40.0) px(cx + x, cy + y, WHITE);
            if (d <= 24.0 && !(x > 0 && std::abs(y) < x * 0.62)) px(cx + x, cy + y, YELLOW); // a Pac-Man-like shape
        }
    px(cx + 6, cy - 12, BLACK);
    px(cx + 7, cy - 12, BLACK);
    px(cx + 6, cy - 11, BLACK);
    px(cx + 7, cy - 11, BLACK);
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 48; i++) px(16 + i, 140 + j, ((i + j) & 1) ? WHITE : BLACK); // 1-pixel checkerboard: should look sharp
    for (int i = 0; i < 16; i++) { px(112, 140 + i, RGB(70, 70, 140)); }
    drawText(80, 140, "FPS " + std::to_string((int)(fps + 0.5)), WHITE);
    drawText(80, 152, "TICK " + std::to_string((int)(tickRate + 0.5)), WHITE);
    drawText(80, 164, "HZ " + std::to_string(refreshHz), WHITE);
    drawText(80, 176, padOk ? "PAD OK" : "PAD NO", padOk ? YELLOW : RGB(160, 160, 160));
    rect(sx - 1, sy - 1, 18, 18, WHITE);
    rect(sx, sy, 16, 16, YELLOW); // the square you move
    drawText(16, 200, "X " + std::to_string(sx), WHITE);
    drawText(16, 212, "Y " + std::to_string(sy), WHITE);
}

int main(int argc, char **argv) {
    bool dumpMode = false;
    std::string dumpPath;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
            dumpMode = true;
            dumpPath = argv[++i];
        }

    std::string log;
    auto note = [&](const std::string &s) { log += s + "\n"; };

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
        note(std::string("SDL_Init failed: ") + SDL_GetError());
        if (!dumpMode) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Port test", SDL_GetError(), nullptr);
            return 1;
        }
    }
    SDL_version v;
    SDL_GetVersion(&v);
    note("Port test (Phase 2)");
    note("SDL version: " + std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch));
    note(std::string("Video driver: ") + (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none"));

    int scale = 2, refreshHz = 0;
    SDL_Rect usable;
    SDL_DisplayMode dm;
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0) scale = std::max(1, std::min(3, (usable.h - 90) / H));
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        refreshHz = dm.refresh_rate;
        note("Display: " + std::to_string(dm.w) + "x" + std::to_string(dm.h) + " at " + std::to_string(dm.refresh_rate) + " Hz");
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_Window *win = SDL_CreateWindow("Pac-Man port: toolchain test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * scale, H * scale, SDL_WINDOW_RESIZABLE);
    if (!win) {
        note(std::string("Window failed: ") + SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(win, W, H);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    bool vsync = ren != nullptr;
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        note("Hardware rendering unavailable; using software.");
    }
    if (!ren) {
        note(std::string("Renderer failed: ") + SDL_GetError());
        return 1;
    }
    SDL_RendererInfo ri;
    SDL_GetRendererInfo(ren, &ri);
    note(std::string("Renderer: ") + ri.name + (ri.flags & SDL_RENDERER_PRESENTVSYNC ? " (vsync on)" : " (vsync off)"));
    SDL_RenderSetLogicalSize(ren, W, H);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);

    SDL_AudioSpec want{}, got{};
    want.freq = AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audioCb;
    SDL_AudioDeviceID adev = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    int beepCount = 0;
    auto beep = [&](double f) {
        if (!adev) return;
        SDL_LockAudioDevice(adev);
        beepFreq = f;
        beepLeft = (int)(0.25 * AUDIO_RATE);
        SDL_UnlockAudioDevice(adev);
        SDL_PauseAudioDevice(adev, 0);
        beepCount++;
    };
    if (adev) note("Audio: opened, " + std::to_string(got.freq) + " Hz, buffer " + std::to_string(got.samples) + " samples");
    else note(std::string("Audio: could not open (") + SDL_GetError() + ")");

    SDL_GameController *pad = nullptr;
    SDL_Joystick *joy = nullptr;
    std::string padName;
    bool padEverUsed = false, keysEverUsed = false, fullscreenToggled = false;
    int sx = 104, sy = 240;
    double fps = 0, tickRate = 0;
    uint64_t frames = 0, ticks = 0, f0 = 0, t0 = 0;
    const uint64_t pf = SDL_GetPerformanceFrequency();
    uint64_t last = SDL_GetPerformanceCounter(), lastReport = last, start = last;
    double acc = 0;
    uint64_t framesTotal = 0, ticksTotal = 0;
    bool running = true, startupBeepDone = false;

    auto openPads = [&]() {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i) && !pad && !joy) {
                pad = SDL_GameControllerOpen(i);
                if (pad) padName = SDL_GameControllerName(pad);
            } else if (!SDL_IsGameController(i) && !pad && !joy) {
                joy = SDL_JoystickOpen(i);
                if (joy) padName = std::string(SDL_JoystickName(joy)) + " (not a standard pad)";
            }
        }
    };
    openPads();
    if (!padName.empty()) note("Gamepad: " + padName);

    int dumpTicks = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;
                else if (k == SDLK_SPACE) beep(880);
                else if (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                    bool fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    fullscreenToggled = true;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                beep(880);
                padEverUsed = true;
            } else if (e.type == SDL_JOYBUTTONDOWN && joy) {
                beep(880);
                padEverUsed = true;
            } else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_JOYDEVICEADDED) {
                openPads();
                if (!padName.empty()) note("Gamepad connected: " + padName);
            } else if (e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEREMOVED) {
                if (pad && !SDL_GameControllerGetAttached(pad)) {
                    SDL_GameControllerClose(pad);
                    pad = nullptr;
                    padName.clear();
                    note("Gamepad removed");
                }
                if (joy && !SDL_JoystickGetAttached(joy)) {
                    SDL_JoystickClose(joy);
                    joy = nullptr;
                    padName.clear();
                    note("Gamepad removed");
                }
                openPads();
            }
        }

        uint64_t now = SDL_GetPerformanceCounter();
        acc += (double)(now - last) / pf;
        last = now;
        if (acc > 0.25) acc = 0.25;
        while (acc >= 1.0 / TICK_HZ) {
            acc -= 1.0 / TICK_HZ;
            ticks++;
            ticksTotal++;
            const Uint8 *ks = SDL_GetKeyboardState(nullptr);
            int dx = 0, dy = 0;
            if (ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A]) dx--;
            if (ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D]) dx++;
            if (ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W]) dy--;
            if (ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S]) dy++;
            if (dx || dy) keysEverUsed = true;
            int pdx = 0, pdy = 0;
            if (pad) {
                if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) pdx--;
                if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) pdx++;
                if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) pdy--;
                if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) pdy++;
                int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
                if (ax < -12000) pdx--;
                if (ax > 12000) pdx++;
                if (ay < -12000) pdy--;
                if (ay > 12000) pdy++;
            } else if (joy) {
                if (SDL_JoystickNumHats(joy) > 0) {
                    Uint8 h = SDL_JoystickGetHat(joy, 0);
                    if (h & SDL_HAT_LEFT) pdx--;
                    if (h & SDL_HAT_RIGHT) pdx++;
                    if (h & SDL_HAT_UP) pdy--;
                    if (h & SDL_HAT_DOWN) pdy++;
                }
                if (SDL_JoystickNumAxes(joy) >= 2) {
                    int ax = SDL_JoystickGetAxis(joy, 0), ay = SDL_JoystickGetAxis(joy, 1);
                    if (ax < -12000) pdx--;
                    if (ax > 12000) pdx++;
                    if (ay < -12000) pdy--;
                    if (ay > 12000) pdy++;
                }
            }
            if (pdx || pdy) padEverUsed = true;
            dx += pdx;
            dy += pdy;
            sx = std::max(10, std::min(W - 26, sx + (dx > 0) - (dx < 0)));
            sy = std::max(10, std::min(H - 26, sy + (dy > 0) - (dy < 0)));
            dumpTicks++;
        }

        drawPattern(sx, sy, fps, tickRate, refreshHz, pad || joy);
        SDL_UpdateTexture(tex, nullptr, fb, W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
        frames++;
        framesTotal++;
        if (!startupBeepDone) {
            beep(660);
            startupBeepDone = true;
        }
        if (!vsync) SDL_Delay(2);

        if (now - lastReport >= pf) {
            double dt = (double)(now - lastReport) / pf;
            fps = (frames - f0) / dt;
            tickRate = (ticks - t0) / dt;
            f0 = frames;
            t0 = ticks;
            lastReport = now;
            std::string t = "Pac-Man port: toolchain test";
            if (!padName.empty()) t += "  |  pad: " + padName;
            SDL_SetWindowTitle(win, t.c_str());
        }
        if (dumpMode && dumpTicks >= 20) {
            SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(fb, W, H, 32, W * 4, SDL_PIXELFORMAT_ARGB8888);
            SDL_SaveBMP(s, dumpPath.c_str());
            SDL_FreeSurface(s);
            running = false;
        }
    }

    double secs = (double)(SDL_GetPerformanceCounter() - start) / pf;
    note("");
    note("Run length: " + std::to_string((int)secs) + " seconds");
    note("Average picture rate: " + std::to_string(framesTotal / secs) + " frames per second");
    note("Average game-tick rate: " + std::to_string(ticksTotal / secs) + " per second (should be about 60.6)");
    note(std::string("Keyboard moved the square: ") + (keysEverUsed ? "yes" : "no"));
    note(std::string("Gamepad used: ") + (padEverUsed ? "yes" : (padName.empty() ? "no gamepad found" : "gamepad found but not used")));
    note("Beeps played: " + std::to_string(beepCount));
    note(std::string("Fullscreen toggled: ") + (fullscreenToggled ? "yes" : "no"));

    if (!dumpMode) {
        std::string path = "port-test-report.txt";
        if (char *base = SDL_GetBasePath()) {
            path = std::string(base) + path;
            SDL_free(base);
        }
        FILE *f = fopen(path.c_str(), "w");
        if (!f) {
            if (char *pref = SDL_GetPrefPath("pacman-port", "test")) {
                path = std::string(pref) + "port-test-report.txt";
                SDL_free(pref);
                f = fopen(path.c_str(), "w");
            }
        }
        if (f) {
            fputs(log.c_str(), f);
            fclose(f);
        }
    } else fputs(log.c_str(), stdout);

    if (adev) SDL_CloseAudioDevice(adev);
    if (pad) SDL_GameControllerClose(pad);
    if (joy) SDL_JoystickClose(joy);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
