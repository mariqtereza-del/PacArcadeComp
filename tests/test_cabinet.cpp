// X45: unit test of the own-picture PNG reader and the generated cabinet. Usage: t_cab DIR (DIR holds the made-up PNG files written by tests/test_cabinet.sh)
#include <cstdio>
#include <string>
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
static std::vector<unsigned char> rd(const std::string &p) {
    std::vector<unsigned char> v;
    if (FILE *f = std::fopen(p.c_str(), "rb")) {
        unsigned char b[4096];
        size_t g;
        while ((g = std::fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + g);
        std::fclose(f);
    }
    return v;
}
int main(int argc, char **argv) {
    using namespace surround;
    const std::string D = argc > 1 ? argv[1] : ".";
    Image im;
    std::string why;
    auto load = [&](const char *name) {
        const auto v = rd(D + "/" + name);
        CHECK(!v.empty(), (std::string("file exists: ") + name).c_str());
        return decode_png(v.data(), v.size(), &im);
    };
    why = load("rgb.png");
    CHECK(why.empty() && im.w == 200 && im.h == 100, "RGB 200x100 read");
    CHECK(im.argb.size() == 20000 && im.argb[0] == 0xFFC81E1Eu && im.argb[199] == 0xFF1E1EC8u && im.argb[100 * 100 + 5] == 0xFFC81E1Eu,
          "RGB pixels right (left red, right blue)");
    why = load("rgba.png");
    CHECK(why.empty() && im.w == 128 && im.h == 96, "RGBA read");
    CHECK(im.argb[0] == 0xFF000000u, "fully transparent pixel shows as black");
    CHECK(im.argb[64] == 0xFF3C783Cu, "opaque pixel kept");
    why = load("gray.png");
    CHECK(why.empty() && im.w == 80 && im.h == 80 && im.argb[0] == 0xFF505050u, "grey read");
    why = load("pal.png");
    CHECK(why.empty() && im.w == 64 && im.h == 64, "palette read");
    why = load("grada.png");
    CHECK(why.empty() && im.w == 96 && im.h == 64, "grey+alpha read");
    why = load("filters.png");
    CHECK(why.empty() && im.w == 100 && im.h == 100, "PNG written with all five row filters read");
    {
        bool ok = true;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                if (im.argb[(size_t)y * 100 + x] != (0xFF000000u | ((uint32_t)(x * 2) << 16) | ((uint32_t)(y * 2) << 8) | (uint32_t)((x + y) & 255))) ok = false;
        CHECK(ok, "filtered picture decodes exactly (gradient)");
    }
    why = load("small.png");
    CHECK(!why.empty() && im.w == 0 && im.argb.empty(), ("too small refused: " + why).c_str());
    why = load("huge.png");
    CHECK(!why.empty() && why.find("too large") != std::string::npos, ("too large (9000 wide) refused: " + why).c_str());
    why = load("corrupt.png");
    CHECK(!why.empty(), ("damaged data refused: " + why).c_str());
    why = load("trunc.png");
    CHECK(!why.empty(), ("cut-off file refused: " + why).c_str());
    why = load("interlaced.png");
    CHECK(why.find("interlaced") != std::string::npos, ("interlaced refused: " + why).c_str());
    why = load("deep.png");
    CHECK(why.find("8 bits") != std::string::npos, ("16-bit refused: " + why).c_str());
    why = load("notpng.png");
    CHECK(why == "not a PNG file", "a text file called .png refused");
    {
        std::vector<unsigned char> e;
        why = decode_png(e.data(), 0, &im);
        CHECK(!why.empty(), "empty file refused");
    }
    {
        std::vector<unsigned char> big(kMaxFileBytes + 1, 0);
        why = decode_png(big.data(), big.size(), &im);
        CHECK(why.find("16 MB") != std::string::npos, "file over 16 MB refused");
    }
    // cover_source: aspect kept, centred
    {
        Rect r = cover_source(1000, 500, 1920, 1080);
        CHECK(r.h == 500 && r.w == 888 && r.x == 56 && r.y == 0, "cover: wide window, picture cropped left and right");
        r = cover_source(500, 1000, 1920, 1080);
        CHECK(r.w == 500 && r.h == 281 && r.x == 0, "cover: tall picture cropped top and bottom");
        r = cover_source(1920, 1080, 1920, 1080);
        CHECK(r.x == 0 && r.y == 0 && r.w == 1920 && r.h == 1080, "cover: same shape, whole picture");
    }
    // the generated cabinet: wording is ours, no forbidden names; rectangles inside the window; deterministic
    {
        const std::string t = marquee_text(), bad = forbidden_words();
        bool clean = true;
        size_t i = 0;
        while (i < bad.size()) {
            size_t j = bad.find(' ', i);
            if (j == std::string::npos) j = bad.size();
            if (t.find(bad.substr(i, j - i)) != std::string::npos) clean = false;
            i = j + 1;
        }
        CHECK(clean && t == "MAZE CHASE", "marquee wording is our own and has no company name");
    }
    struct S {
        int w, h;
        Rect pic;
    } sh[] = {{1024, 768, {288, 96, 448, 576}}, {1920, 1080, {624, 108, 672, 864}}, {2560, 1080, {944, 108, 672, 864}},
              {500, 400, {89, 0, 311, 400}},    {704, 576, {0, 0, 704, 576}},       {3840, 2160, {1248, 216, 1344, 1728}}};
    for (const S &s : sh) {
        std::vector<uint32_t> a((size_t)s.w * s.h), b((size_t)s.w * s.h);
        render_cabinet(a.data(), s.w, s.h, s.pic);
        render_cabinet(b.data(), s.w, s.h, s.pic);
        const std::string tag = std::to_string(s.w) + "x" + std::to_string(s.h) + ": ";
        CHECK(a == b, (tag + "drawing is deterministic").c_str());
        bool op = true;
        for (uint32_t v : a)
            if ((v >> 24) != 255) {
                op = false;
                break;
            }
        CHECK(op, (tag + "every pixel opaque").c_str());
        bool blk = true;
        for (int y = s.pic.y; y < s.pic.y + s.pic.h && y < s.h; ++y)
            for (int x = s.pic.x; x < s.pic.x + s.pic.w && x < s.w; ++x)
                if (a[(size_t)y * s.w + x] != 0xFF000000u) blk = false;
        CHECK(blk, (tag + "picture rectangle stays black").c_str());
        const Parts q = parts(s.w, s.h, s.pic);
        auto in = [&](Rect r) { return r.empty() || (r.x >= 0 && r.y >= 0 && r.x + r.w <= s.w && r.y + r.h <= s.h); };
        auto apart = [&](Rect r) { return r.empty() || r.y + r.h <= s.pic.y || r.y >= s.pic.y + s.pic.h; };
        CHECK(in(q.marquee) && in(q.grille) && apart(q.marquee) && apart(q.grille), (tag + "marquee and grille inside the window, not on the picture").c_str());
    }
    {
        const Parts q = parts(1920, 1080, {624, 108, 672, 864});
        CHECK(!q.marquee.empty() && !q.grille.empty(), "1920x1080: marquee and grille present");
    }
    {
        const Parts q = parts(704, 576, {0, 0, 704, 576});
        CHECK(q.marquee.empty() && q.grille.empty(), "no room around the picture: no marquee, no grille");
    }
    {
        const Parts q = parts(500, 400, {89, 0, 311, 400});
        CHECK(q.marquee.empty() && q.grille.empty(), "picture fills the height: no marquee, no grille");
    }
    for (char ch = 'A'; ch <= 'Z'; ++ch) { CHECK(std::strlen(glyph(ch)) == 35, "every capital has a 5x7 glyph"); }
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
