// X44 unit test: the rectangles around the picture at several window shapes (using the real layout from display.cpp), and the cabinet look's fills (surround.h).
#include "../src/app/display.h"
#include "../src/app/surround.h"
#include "../src/app/display.cpp"
#include <cstdio>
#include <string>
static int fails = 0, n = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
using surround::Rect;
static bool overlap(const Rect &a, const Rect &b) { return !a.empty() && !b.empty() && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static bool inside(const Rect &a, int w, int h) { return a.empty() || (a.x >= 0 && a.y >= 0 && a.x + a.w <= w && a.y + a.h <= h); }
int main() {
    struct Shape {
        const char *name;
        int w, h;
    } shapes[] = {{"4:3 1024x768", 1024, 768},    {"16:9 1920x1080", 1920, 1080}, {"21:9 2560x1080", 2560, 1080},
                  {"21:9 3440x1440", 3440, 1440}, {"tall 600x1000", 600, 1000},   {"exact 672x864", 672, 864},
                  {"small 300x400", 300, 400},    {"tiny 100x120", 100, 120},     {"square 900x900", 900, 900}};
    const int contents[] = {224, 352};
    for (const Shape &s : shapes)
        for (int cw : contents)
            for (int fit = 0; fit < 2; ++fit) {
                const display::Layout L = display::compute_layout(s.w, s.h, cw, 288, fit ? display::Fit::Fill : display::Fit::Integer);
                const Rect pic{L.dest.x, L.dest.y, L.dest.w, L.dest.h};
                const surround::Around a = surround::around(s.w, s.h, pic);
                const Rect parts[5] = {a.top, a.bottom, a.left, a.right, pic};
                bool ok = true;
                for (int i = 0; i < 5; ++i) {
                    if (!inside(parts[i], s.w, s.h)) ok = false;
                    for (int j = i + 1; j < 5; ++j)
                        if (overlap(parts[i], parts[j])) ok = false;
                }
                long long sum = 0;
                for (const Rect &r : parts) sum += r.area();
                char msg[160];
                std::snprintf(msg, sizeof msg, "%s content %d %s: the four rectangles and the picture do not overlap, stay in the window", s.name, cw,
                              fit ? "fill" : "integer");
                CHECK(ok, msg);
                std::snprintf(msg, sizeof msg, "%s content %d %s: together they cover the window exactly (%lld of %lld)", s.name, cw, fit ? "fill" : "integer", sum,
                              (long long)s.w * s.h);
                CHECK(sum == (long long)s.w * s.h, msg);
                // the picture is centred, so left/right and top/bottom are (nearly) equal
                CHECK(std::abs(a.left.w - a.right.w) <= 1 && std::abs(a.top.h - a.bottom.h) <= 1, "centred picture: equal margins (within 1 pixel)");
                // layout untouched by this step: the picture size is what compute_layout says (nothing in surround.h changes it)
                CHECK(pic.w == L.dest.w && pic.h == L.dest.h, "picture rectangle is the layout's rectangle");
                // wide windows have side areas, tall windows top/bottom areas
                if (s.w * 288 > s.h * cw + 2 * cw) CHECK(a.left.w > 0 && a.right.w > 0, "a window wider than the picture has side areas");
                {
                    std::vector<uint32_t> px((size_t)s.w * s.h, 0);
                    surround::render_cabinet(px.data(), s.w, s.h, pic);
                    bool opaque = true;
                    for (uint32_t v : px)
                        if ((v >> 24) != 255) {
                            opaque = false;
                            break;
                        }
                    CHECK(opaque, "cabinet picture: every pixel opaque");
                }
                const Rect o = surround::bezel_outer(s.w, s.h, pic);
                {
                    const int cx0 = std::max(0, pic.x), cy0 = std::max(0, pic.y), cx1 = std::min(s.w, pic.x + pic.w), cy1 = std::min(s.h, pic.y + pic.h);
                    CHECK(o.x <= cx0 && o.y <= cy0 && o.x + o.w >= cx1 && o.y + o.h >= cy1, "bezel contains the visible part of the picture");
                }
                CHECK(inside(o, s.w, s.h), "bezel inside the window");
            }
    {
        const int W = 1920, H = 1080;
        const display::Layout L = display::compute_layout(W, H, 224, 288, display::Fit::Integer);
        const Rect pic{L.dest.x, L.dest.y, L.dest.w, L.dest.h};
        const surround::Around a = surround::around(W, H, pic);
        std::printf("1920x1080 integer layout: picture %d,%d %dx%d; left area %d wide, right area %d wide, top %d, bottom %d\n", pic.x, pic.y, pic.w, pic.h, a.left.w,
                    a.right.w, a.top.h, a.bottom.h);
        CHECK(a.left.w > 400 && a.right.w > 400, "16:9: wide side areas");
    }
    // 21:9 and 4:3 shapes
    {
        const display::Layout L = display::compute_layout(2560, 1080, 224, 288, display::Fit::Fill);
        const surround::Around a = surround::around(2560, 1080, {L.dest.x, L.dest.y, L.dest.w, L.dest.h});
        CHECK(a.left.w > 700 && a.right.w > 700 && a.top.h == 0, "21:9: very wide side areas, no top area");
    }
    {
        const display::Layout L = display::compute_layout(1024, 768, 224, 288, display::Fit::Fill);
        const surround::Around a = surround::around(1024, 768, {L.dest.x, L.dest.y, L.dest.w, L.dest.h});
        CHECK(a.left.w > 100 && a.right.w > 100 && a.top.h == 0, "4:3: side areas, no top area");
    }
    // helpers
    CHECK(std::string(surround::name(0)) == "black" && std::string(surround::name(1)) == "cabinet" && surround::from_name("cabinet") == 1 &&
              surround::from_name("black") == 0 && surround::from_name(nullptr) == 0,
          "names");
    CHECK(surround::bezel_width({0, 0, 10, 100}) == 3 && surround::bezel_width({0, 0, 10, 10}) == 2 && surround::bezel_width({0, 0, 10, 2000}) == 24,
          "bezel width 3 % of the height, 2 to 24");
    // an empty/degenerate picture does not crash and everything stays inside
    {
        const surround::Around a = surround::around(500, 400, {0, 0, 0, 0});
        CHECK(a.top.area() + a.bottom.area() + a.left.area() + a.right.area() <= 500LL * 400, "empty picture: the areas stay inside the window");
        std::vector<uint32_t> px(500 * 400);
        surround::render_cabinet(px.data(), 500, 400, {0, 0, 0, 0});
        CHECK(true, "empty picture: the cabinet still renders without a crash");
    }
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
