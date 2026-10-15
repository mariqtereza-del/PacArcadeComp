// X43 unit test: the CRT look presets (crt.h table) and the CRT LOOK menu value (menu.cpp).
#include <SDL.h>
#include "../src/board/romset.h"
#include "../src/video/gfx.h"
#include "../src/app/controls.cpp"
#include "../src/app/overlay.cpp"
#include "../src/app/menu.cpp"
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
int main() {
    using namespace crt;
    CHECK(kPresetCount == 4, "four presets");
    const struct {
        const char *name;
        int mask, glow, curve;
        bool scan;
    } want[4] = {{"OFF", 0, 0, 0, false}, {"SUBTLE", 1, 1, 0, false}, {"ARCADE MONITOR", 1, 1, 0, true}, {"OLD TV", 2, 2, 2, false}};
    for (int i = 0; i < 4; ++i) {
        const Preset &p = preset(i);
        CHECK(std::string(p.name) == want[i].name && p.mask == want[i].mask && p.glow == want[i].glow && p.curve == want[i].curve && p.scanlines == want[i].scan,
              want[i].name);
        CHECK(preset_match(p.mask, p.glow, p.curve, p.scanlines) == i, "each preset matches itself and no other");
        CHECK(std::string(look_name(p.mask, p.glow, p.curve, p.scanlines)) == want[i].name, "look_name of a preset");
        CHECK(std::string(p.name).size() <= 14, "name fits the menu");
    }
    CHECK(preset_match(1, 0, 0, false) == -1 && std::string(look_name(1, 0, 0, false)) == "CUSTOM", "a single changed setting reads CUSTOM");
    CHECK(preset_match(0, 0, 0, true) == -1, "scanlines alone is CUSTOM");
    CHECK(preset_match(2, 2, 2, true) == -1, "Old TV plus scanlines is CUSTOM");
    // stepping: forward through all and back to the start; backward; custom
    {
        int m = 0, g = 0, c = 0;
        bool s = false;
        std::string order;
        for (int k = 0; k < 5; ++k) {
            const int i = preset_step(m, g, c, s, +1);
            const Preset &p = preset(i);
            m = p.mask;
            g = p.glow;
            c = p.curve;
            s = p.scanlines;
            order += std::string(p.name) + ",";
        }
        CHECK(order == "SUBTLE,ARCADE MONITOR,OLD TV,OFF,SUBTLE,", "forward: Off > Subtle > Arcade monitor > Old TV > Off > ...");
        order.clear();
        for (int k = 0; k < 4; ++k) {
            const int i = preset_step(m, g, c, s, -1);
            const Preset &p = preset(i);
            m = p.mask;
            g = p.glow;
            c = p.curve;
            s = p.scanlines;
            order += std::string(p.name) + ",";
        }
        CHECK(order == "OFF,OLD TV,ARCADE MONITOR,SUBTLE,", "backward is the reverse");
    }
    CHECK(preset_step(1, 0, 0, false, +1) == 0 && preset_step(1, 0, 0, false, -1) == 3, "from CUSTOM: forward = OFF, backward = OLD TV");
    CHECK(std::string(preset(-1).name) == "OFF" && std::string(preset(99).name) == "OFF", "out-of-range index is safe");
    // the menu value
    menu::Model m;
    CHECK(menu::value_text(menu::Cmd::CrtLook, m) == "OFF", "default look OFF");
    m.crt_mask = 1;
    m.glow = 1;
    CHECK(menu::value_text(menu::Cmd::CrtLook, m) == "SUBTLE", "mask light + glow light = SUBTLE");
    m.scanlines = true;
    CHECK(menu::value_text(menu::Cmd::CrtLook, m) == "ARCADE MONITOR", "plus scanlines = ARCADE MONITOR");
    m.curve = 1;
    CHECK(menu::value_text(menu::Cmd::CrtLook, m) == "CUSTOM", "plus a curve = CUSTOM");
    m.crt_mask = 2;
    m.glow = 2;
    m.curve = 2;
    m.scanlines = false;
    CHECK(menu::value_text(menu::Cmd::CrtLook, m) == "OLD TV", "all strong = OLD TV");
    CHECK(!menu::locked(menu::Cmd::CrtLook, m), "free outside Purist");
    m.purist = true;
    CHECK(menu::locked(menu::Cmd::CrtLook, m) && menu::value_text(menu::Cmd::CrtLook, m) == "OFF", "locked in Purist, shown OFF");
    std::printf("%d checks, %d failed\n%s\n", n, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
