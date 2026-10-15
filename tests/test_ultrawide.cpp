// X46: unit test of surround::ambient_grid and the mode names.
#include <cstdio>
#include <vector>
#include "app/surround.h"
static int n = 0, fails = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
int main() {
    using namespace surround;
    CHECK(from_name("ambient") == kAmbient && std::string(name(kAmbient)) == "ambient" && from_name("cabinet") == kCabinet && from_name("black") == kBlack &&
              from_name(nullptr) == kBlack,
          "names");
    CHECK(next_mode(0) == 1 && next_mode(1) == 2 && next_mode(2) == 0, "cycle black, cabinet, ambient, black");
    auto dark = [](uint32_t v) {
        const unsigned k = 255 - kAmbientDarkAlpha;
        return 0xFF000000u | (((v >> 16) & 255) * k / 255) << 16 | (((v >> 8) & 255) * k / 255) << 8 | ((v & 255) * k / 255);
    };
    const int W = 224, H = 288, S = 352;
    std::vector<uint32_t> src((size_t)S * H, 0xFFFFFFFFu), g((size_t)kAmbientW * kAmbientH);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) src[(size_t)y * S + x] = 0xFF204080u; // pixels outside the picture (x >= 224) stay white and must not leak in
    ambient_grid(src.data(), S, W, H, kAmbientW, kAmbientH, g.data());
    bool same = true;
    for (uint32_t v : g)
        if (v != dark(0xFF204080u)) same = false;
    CHECK(same, "uniform picture gives the same colour in every cell, nothing from outside the picture");
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) src[(size_t)y * S + x] = x < 112 ? 0xFFFF0000u : 0xFF0000FFu;
    ambient_grid(src.data(), S, W, H, kAmbientW, kAmbientH, g.data());
    CHECK(g[0] == dark(0xFFFF0000u) && g[kAmbientW - 1] == dark(0xFF0000FFu) && g[(size_t)(kAmbientH - 1) * kAmbientW] == dark(0xFFFF0000u),
          "left half red, right half blue");
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) src[(size_t)y * S + x] = (x + y) & 1 ? 0xFFFFFFFFu : 0xFF000000u;
    ambient_grid(src.data(), S, W, H, kAmbientW, kAmbientH, g.data());
    {
        bool grey = true;
        for (uint32_t v : g) {
            const int r = (v >> 16) & 255;
            if (r < (int)(127 * (255 - kAmbientDarkAlpha) / 255) - 3 || r > (int)(127 * (255 - kAmbientDarkAlpha) / 255) + 3 || (v & 255) != (unsigned)r) grey = false;
        }
        CHECK(grey, "a fine black/white pattern averages to mid grey (blur)");
    }
    CHECK(kAmbientDarkAlpha > 0 && kAmbientDarkAlpha < 255, "darkening between 0 and 255");
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
