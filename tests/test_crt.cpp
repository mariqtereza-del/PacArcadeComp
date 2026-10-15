// X40 unit test: the shadow-mask pattern and its brightness average (crt.h).
#include "../src/app/crt.h"
#include <cstdio>
#include <vector>
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
int main() {
    CHECK(crt::from_name("off") == 0 && crt::from_name("light") == 1 && crt::from_name("strong") == 2 && crt::from_name(nullptr) == 0 && crt::from_name("x") == 0,
          "names");
    CHECK(std::string(crt::name(0)) == "off" && std::string(crt::name(1)) == "light" && std::string(crt::name(2)) == "strong", "name()");
    CHECK(crt::dim(0) == 255 && crt::dim(1) == 215 && crt::dim(2) == 150, "dim");
    for (int level = 1; level <= 2; ++level) {
        const int k = crt::dim(level), W = 99, H = 4;
        std::vector<uint32_t> m(W * H);
        crt::fill(m.data(), W, H, level);
        double sum = 0;
        bool pat = true;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                uint32_t p = m[y * W + x];
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                int er = x % 3 == 0 ? 255 : k, eg = x % 3 == 1 ? 255 : k, eb = x % 3 == 2 ? 255 : k;
                if (r != er || g != eg || b != eb || (p >> 24) != 255) pat = false;
                if (p != m[x]) pat = false; // every row equal
                sum += (r + g + b) / 3.0 / 255.0;
            }
        CHECK(pat, "stripe pattern red/green/blue, every row equal");
        double avg = sum / (W * H);
        std::printf("level %s: measured average %.4f, formula %.4f\n", crt::name(level), avg, crt::average(level));
        CHECK(avg > crt::average(level) - 0.0001 && avg < crt::average(level) + 0.0001, "average matches formula");
    }
    CHECK(crt::average(1) > 0.88 && crt::average(1) < 0.90 && crt::average(2) > 0.72 && crt::average(2) < 0.73, "89 % and 72.5 %");
    CHECK(crt::average(2) < crt::average(1) && crt::average(1) < 1.0, "strong darker than light");
    std::printf("%d checks, %d failed\n", n, fails);
    std::printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
