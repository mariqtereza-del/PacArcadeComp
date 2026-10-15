// X42 unit test: the curve table (centre untouched, symmetric, inside the screen, inverse) and the vignette picture (crt.h).
#include "../src/app/crt.h"
#include <cmath>
#include <cstdio>
static int fails = 0, n = 0;
#define CHECK(c, m)                                                                                                                                                      \
    do {                                                                                                                                                                 \
        ++n;                                                                                                                                                             \
        if (!(c)) {                                                                                                                                                      \
            ++fails;                                                                                                                                                     \
            std::printf("FAIL: %s\n", m);                                                                                                                                \
        }                                                                                                                                                                \
    } while (0)
int main() {
    for (int level = 0; level <= 2; ++level) {
        const double k = crt::curve_k(level);
        double sx, sy;
        crt::curve_map(k, 0, 0, &sx, &sy);
        CHECK(sx == 0 && sy == 0, "the centre does not move");
        crt::curve_map(k, 1e-6, 0, &sx, &sy);
        CHECK(std::fabs(sx / 1e-6 - 1.0) < 1e-6, "scale at the centre is exactly 1");
        crt::curve_map(k, 1, 0, &sx, &sy);
        CHECK(std::fabs(sx - (1 + k) / (1 + 2 * k)) < 1e-12 && sy == 0, "middle of an edge");
        crt::curve_map(k, 1, 1, &sx, &sy);
        CHECK(std::fabs(sx - 1 / (1 + 2 * k)) < 1e-12 && sx == sy, "corner pulled in");
        // symmetric: (-p) -> (-s), mirror in x and y
        bool sym = true, inside = true, mono = true;
        for (double y = -1; y <= 1.0001; y += 0.125)
            for (double x = -1; x <= 1.0001; x += 0.125) {
                double a, b, c, d;
                crt::curve_map(k, x, y, &a, &b);
                crt::curve_map(k, -x, y, &c, &d);
                if (std::fabs(a + c) > 1e-12 || std::fabs(b - d) > 1e-12) sym = false;
                crt::curve_map(k, x, -y, &c, &d);
                if (std::fabs(a - c) > 1e-12 || std::fabs(b + d) > 1e-12) sym = false;
                if (std::fabs(a) > 1 + 1e-12 || std::fabs(b) > 1 + 1e-12) inside = false;
                double px, py;
                crt::curve_unmap(k, a, b, &px, &py);
                if (std::fabs(px - x) > 1e-9 || std::fabs(py - y) > 1e-9) {
                    std::printf("  unmap (%g,%g) -> (%g,%g)\n", x, y, px, py);
                    sym = false;
                }
            }
        CHECK(sym, "symmetric in x and y, and curve_unmap is the exact inverse");
        CHECK(inside, "everything stays inside the screen rectangle");
        for (double x = 0; x < 1; x += 0.01) {
            double a, b, c, d;
            crt::curve_map(k, x, 0.5, &a, &b);
            crt::curve_map(k, x + 0.01, 0.5, &c, &d);
            if (c <= a) mono = false;
        }
        CHECK(mono, "monotonic along a row (the picture is not folded)");
        // the mesh table
        std::vector<crt::MeshVertex> v;
        std::vector<int> idx;
        const int nx = 28, ny = 36;
        crt::build_curve_mesh(v, idx, nx, ny, 100, 50, 672, 864, k, 0, 0, 0.636f, 1.0f);
        CHECK((int)v.size() == (nx + 1) * (ny + 1) && (int)idx.size() == nx * ny * 6, "mesh sizes");
        bool msym = true, min_in = true, uv_ok = true;
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i) {
                const crt::MeshVertex &a = v[j * (nx + 1) + i], &b = v[(ny - j) * (nx + 1) + (nx - i)];
                if (std::fabs((a.x - 100 - 336) + (b.x - 100 - 336)) > 1e-3 || std::fabs((a.y - 50 - 432) + (b.y - 50 - 432)) > 1e-3) msym = false;
                if (a.x < 100 - 1e-3 || a.x > 772 + 1e-3 || a.y < 50 - 1e-3 || a.y > 914 + 1e-3) min_in = false;
                if (std::fabs(a.u - 0.636f * i / nx) > 1e-6 || std::fabs(a.v - 1.0f * j / ny) > 1e-6) uv_ok = false;
            }
        CHECK(msym, "the mesh table is point-symmetric about the centre");
        CHECK(min_in, "all mesh points inside the destination rectangle");
        CHECK(uv_ok, "texture coordinates are the plain grid");
        const crt::MeshVertex &mid = v[(ny / 2) * (nx + 1) + nx / 2];
        CHECK(std::fabs(mid.x - 436) < 1e-3 && std::fabs(mid.y - 482) < 1e-3, "the centre vertex stays at the centre of the rectangle");
        if (level == 0) {
            bool ident = true;
            for (int j = 0; j <= ny; ++j)
                for (int i = 0; i <= nx; ++i) {
                    const crt::MeshVertex &a = v[j * (nx + 1) + i];
                    if (std::fabs(a.x - (100 + 672.0 * i / nx)) > 1e-3 || std::fabs(a.y - (50 + 864.0 * j / ny)) > 1e-3) ident = false;
                }
            CHECK(ident, "level off: the table is the plain grid");
        }
        // vignette
        const int W = 64, H = 80;
        std::vector<uint32_t> vg((size_t)W * H);
        crt::vignette_fill(vg.data(), W, H, level);
        const double a = crt::vignette_a(level);
        auto g = [&](int x, int y) { return (int)(vg[(size_t)y * W + x] & 255); };
        CHECK(g(W / 2, H / 2) >= 254 && g(W / 2 - 1, H / 2 - 1) >= 254, "vignette: the centre is not darkened");
        CHECK(std::abs(g(0, 0) - (int)(255 * (1 - a) + 0.5)) <= 5, "vignette: the corner has about 1 - a (pixel centres are half a pixel inside the corner)");
        bool vs = true, vm = true;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                if (vg[y * W + x] != vg[(H - 1 - y) * W + x] || vg[y * W + x] != vg[y * W + (W - 1 - x)]) vs = false;
                if (x > 0 && x < W / 2 && g(x, y) < g(x - 1, y)) vm = false;
            }
        CHECK(vs, "vignette symmetric");
        CHECK(vm, "vignette gets brighter towards the middle");
        bool gray = true;
        for (uint32_t p : vg)
            if (((p >> 16) & 255) != (p & 255) || (p >> 24) != 255) gray = false;
        CHECK(gray, "vignette is grey with alpha 255");
        std::printf("level %d: k %.2f, edge-middle %.4f, corner %.4f, vignette corner %d\n", level, k, (1 + k) / (1 + 2 * k), 1 / (1 + 2 * k), g(0, 0));
    }
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
