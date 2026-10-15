// X07: the last-device tracker on made-up events.
#include <cstdio>
#include <string>
#include "../src/app/lastdevice.h"
using namespace lastdevice;
static int fails = 0, checks = 0;
static void chk(const char *what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what, got, want);
    }
}
int main() {
    Tracker t;
    chk("starts with no device", (long)t.device(), (long)Device::None);
    chk("a key: change", t.key(), 1);
    chk("device is keyboard", (long)t.device(), (long)Device::Keyboard);
    chk("another key: no change", t.key(), 0);
    chk("tiny mouse jitter (1,1) is ignored", t.mouse_motion(1, 1), 0);
    chk("still keyboard", (long)t.device(), (long)Device::Keyboard);
    chk("mouse move (3,0) counts", t.mouse_motion(3, 0), 1);
    chk("device is mouse", (long)t.device(), (long)Device::Mouse);
    chk("more mouse: no change", t.mouse_motion(10, 10), 0);
    chk("a key after the mouse: back to the keyboard", t.key(), 1);
    chk("mouse click", t.mouse_click(), 1);
    chk("click again", t.mouse_click(), 0);
    chk("mouse wheel (still mouse)", t.mouse_wheel(), 0);
    chk("wheel after keyboard", (t.key(), t.mouse_wheel()), 1);
    chk("pad button", t.pad_button(), 1);
    chk("device is pad", (long)t.device(), (long)Device::Pad);
    chk("pad button again", t.pad_button(), 0);
    chk("stick at rest (200)", t.key() && !t.pad_stick(200), 1);
    chk("stick drift just under the dead zone (12000) does not count", t.pad_stick(12000) || t.pad_stick(-12000), 0);
    chk("still keyboard after drift", (long)t.device(), (long)Device::Keyboard);
    chk("stick pushed right (20000)", t.pad_stick(20000), 1);
    chk("device is pad", (long)t.device(), (long)Device::Pad);
    chk("stick pushed left (-32768)", (t.key(), t.pad_stick(-32768)), 1);
    chk("pad type set while the keyboard is last: not a change of device", (t.key(), t.pad_type(4)), 0);
    chk("type remembered", t.pad_type(), 4);
    chk("same type again: no change", t.pad_type(4), 0);
    chk("pad type changes while a pad is last: a change", (t.pad_button(), t.pad_type(7)), 1);
    chk("new type", t.pad_type(), 7);
    chk("pad is last", t.pad_is_last(), 1);
    chk("pad removed (type 0) while the pad is last: a change", t.pad_type(0), 1);
    unsigned c = t.changes();
    t.reset();
    chk("reset: no device", (long)t.device(), (long)Device::None);
    chk("reset: change count 0", t.changes(), 0);
    chk("changes were counted before", c > 10, 1);
    chk("names", std::string(name(Device::Keyboard)) + name(Device::Mouse) + name(Device::Pad) + name(Device::None) == "KEYBOARDMOUSEPADNONE", 1);
    // a play sequence: key, key, mouse, pad, pad, key -> 4 changes
    {
        Tracker s;
        s.key();
        s.key();
        s.mouse_click();
        s.pad_button();
        s.pad_button();
        s.key();
        chk("play sequence: 4 changes", s.changes(), 4);
    }
    // X08: the hint set for every last-device value and every setting
    {
        Tracker h;
        chk("hints: keyboard before any input", (long)h.hint_device(), (long)Device::Keyboard);
        h.pad_button();
        chk("hints: pad after a pad button", (long)h.hint_device(), (long)Device::Pad);
        h.mouse_click();
        h.mouse_motion(50, 0);
        h.mouse_wheel();
        chk("hints: the mouse does not change the hint set", (long)h.hint_device(), (long)Device::Pad);
        h.key();
        chk("hints: keyboard again after a key", (long)h.hint_device(), (long)Device::Keyboard);
        h.pad_stick(30000);
        chk("hints: pad after the stick", (long)h.hint_device(), (long)Device::Pad);
        for (int setting = 0; setting < 3; ++setting)
            for (int d = 0; d < 2; ++d)
                for (int present = 0; present < 2; ++present) {
                    bool want = present && (setting == 2 || (setting == 0 && d == 1));
                    chk(("hints_use_pad setting " + std::to_string(setting) + " device " + (d ? "pad" : "keyboard") + " pad " + (present ? "present" : "absent")).c_str(),
                        hints_use_pad(setting, d ? Device::Pad : Device::Keyboard, present != 0), want);
                }
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
