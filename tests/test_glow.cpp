// X41 unit test on known images: the glow (bloom) in crt.h.
#include "../src/app/crt.h"
#include <cstdio>
#include <string>
#include <vector>
static int fails = 0, n = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
static int R(uint32_t p) { return (p >> 16) & 255; }
static int G(uint32_t p) { return (p >> 8) & 255; }
static int B(uint32_t p) { return p & 255; }
int main() {
    const int W = 41, H = 41, S = 50; // stride deliberately larger than the width (like the 352-wide texture)
    std::vector<uint32_t> src((size_t)S * H, 0xFF000000u), out((size_t)W * H);
    // 1. off: black
    src[20 * S + 20] = 0xFFFFFFFFu;
    crt::glow(src.data(), S, W, H, 0, out.data());
    {
        bool allb = true;
        for (uint32_t p : out)
            if (p != 0xFF000000u) allb = false;
        CHECK(allb, "level off gives black");
    }
    // 2. one white pixel: symmetric, peak in the middle, falls off, zero far away, alpha 255
    for (int level = 1; level <= 2; ++level) {
        crt::glow(src.data(), S, W, H, level, out.data());
        const int r = crt::glow_radius(level);
        CHECK(R(out[20 * W + 20]) > 0 && R(out[20 * W + 20]) == G(out[20 * W + 20]) && G(out[20 * W + 20]) == B(out[20 * W + 20]),
              "white pixel glows grey-white in the middle");
        bool sym = true, mono = true, alpha = true;
        for (int d = 1; d <= 2 * r; ++d) {
            if (out[20 * W + 20 + d] != out[20 * W + 20 - d] || out[(20 + d) * W + 20] != out[(20 - d) * W + 20] || out[20 * W + 20 + d] != out[(20 + d) * W + 20])
                sym = false;
            if (R(out[20 * W + 20 + d]) > R(out[20 * W + 20 + d - 1])) mono = false;
        }
        for (uint32_t p : out)
            if ((p >> 24) != 255) alpha = false;
        CHECK(sym, "symmetric left/right/up/down");
        CHECK(mono, "falls off away from the pixel");
        CHECK(alpha, "alpha 255");
        CHECK(out[20 * W + 20 + 2 * r + 1] == 0xFF000000u && out[(20 + 2 * r + 1) * W + 20] == 0xFF000000u, "zero beyond twice the radius");
        CHECK(out[0] == 0xFF000000u && out[W * H - 1] == 0xFF000000u, "corners stay black");
        double sum = 0;
        for (uint32_t p : out) sum += R(p); // blur keeps the energy: 255 * gain (rounding per pixel)
        std::printf("level %d: peak %d, total %.0f, expected about %.0f\n", level, R(out[20 * W + 20]), sum, 255.0 * crt::glow_gain(level));
        CHECK(sum > 0.8 * 255 * crt::glow_gain(level) && sum < 1.2 * 255 * crt::glow_gain(level), "energy kept (within 20 %, rounding)");
    }
    {
        std::vector<uint32_t> l(W * H), s(W * H);
        crt::glow(src.data(), S, W, H, 1, l.data());
        crt::glow(src.data(), S, W, H, 2, s.data());
        {
            double ls = 0, ss = 0;
            for (uint32_t p : l) ls += R(p);
            for (uint32_t p : s) ss += R(p);
            CHECK(ss > ls, "strong glows more than light (total)");
        }
        CHECK(s[20 * W + 20 + 4] != 0xFF000000u && l[20 * W + 20 + 5] == 0xFF000000u, "strong reaches further than light");
    }
    // 3. colour: a pure red pixel glows red only
    {
        std::vector<uint32_t> t((size_t)S * H, 0xFF000000u), o(W * H);
        t[20 * S + 20] = 0xFFFF0000u;
        crt::glow(t.data(), S, W, H, 2, o.data());
        CHECK(R(o[20 * W + 21]) > 0 && G(o[20 * W + 21]) == 0 && B(o[20 * W + 21]) == 0, "red pixel glows red only");
    }
    // 4. dim pictures (every colour at or below the threshold) do not glow
    {
        std::vector<uint32_t> t((size_t)S * H, 0xFF606060u), o(W * H);
        crt::glow(t.data(), S, W, H, 2, o.data());
        bool z = true;
        for (uint32_t p : o)
            if (p != 0xFF000000u) z = false;
        CHECK(z, "grey 96 (the threshold) gives no glow");
    }
    // 5. a full white picture glows evenly in the middle, fades at the edges (edges count as black)
    {
        std::vector<uint32_t> t((size_t)S * H, 0xFFFFFFFFu), o(W * H);
        crt::glow(t.data(), S, W, H, 2, o.data());
        const int mid = R(o[20 * W + 20]);
        CHECK(mid == std::min(255, (int)(255 * crt::glow_gain(2) + 0.5)), "white picture: middle = full gain (clipped at 255)");
        CHECK(R(o[0]) < mid, "corner fades");
    }
    // 6. the source is not changed
    {
        std::vector<uint32_t> t((size_t)S * H, 0xFFFFFFFFu), c = t, o(W * H);
        crt::glow(t.data(), S, W, H, 2, o.data());
        CHECK(t == c, "source untouched");
    }
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
