// X07: which device was used last. A tiny tracker: a keyboard key, a mouse movement or click, a pad button or a pad stick pushed beyond a dead zone each make
// that device "the last one used". Anything that draws hints (the button pictures that switch between keyboard and pad, X08) asks device(). It only watches
// input events: it can never touch the machine. Purist mode does not use it.
#pragma once
#include <cstdlib>
#include <string>

namespace lastdevice {

enum class Device { None, Keyboard, Mouse, Pad };
inline const char *name(Device d) { return d == Device::Keyboard ? "KEYBOARD" : d == Device::Mouse ? "MOUSE" : d == Device::Pad ? "PAD" : "NONE"; }

constexpr int kStickDeadZone = 12000; // of 32767: a stick has to be pushed about 37 % to count (resting sticks drift a little)
constexpr int kMouseMinMove = 3;      // pixels (|dx| + |dy| of one motion event): a bump of the table does not count

class Tracker {
public:
    // Every call returns true when the last device (or, for a pad, its type) changed because of it.
    bool key() { return set(Device::Keyboard); }
    bool mouse_motion(int dx, int dy) { return std::abs(dx) + std::abs(dy) >= kMouseMinMove ? set(Device::Mouse) : false; }
    bool mouse_click() { return set(Device::Mouse); }
    bool mouse_wheel() { return set(Device::Mouse); }
    bool pad_button() { return set(Device::Pad); }
    bool pad_stick(int axis_value) { return std::abs(axis_value) > kStickDeadZone ? set(Device::Pad) : false; } // sticks only (the triggers arrive as buttons)
    // The type of the pad in use (menu::PadType as an int; 0 = none). A change while a pad is the last device counts as a change.
    bool pad_type(int t) {
        if (t == pad_type_) return false;
        pad_type_ = t;
        return dev_ == Device::Pad;
    }
    Device device() const { return dev_; }
    // For hints (X08): the last of keyboard / pad. The mouse does not change it (clicking in a menu with the mouse leaves the hints as they were); keyboard before any input.
    Device hint_device() const { return hint_ == Device::Pad ? Device::Pad : Device::Keyboard; }
    int pad_type() const { return pad_type_; }
    bool pad_is_last() const { return dev_ == Device::Pad; }
    unsigned changes() const { return changes_; }
    void reset() {
        dev_ = Device::None;
        hint_ = Device::Keyboard;
        changes_ = 0;
    }

private:
    bool set(Device d) {
        if (d == Device::Keyboard || d == Device::Pad) hint_ = d;
        if (d == dev_) return false;
        dev_ = d;
        ++changes_;
        return true;
    }
    Device hint_ = Device::Keyboard;
    Device dev_ = Device::None;
    int pad_type_ = 0;
    unsigned changes_ = 0;
};

// Which hint set to draw: setting 0 = AUTO (follow the last device), 1 = KEYBOARD, 2 = PAD. A pad hint set needs a pad to be there (otherwise the keyboard set).
inline bool hints_use_pad(int setting, Device hint_device, bool pad_present) {
    if (!pad_present) return false;
    return setting == 2 || (setting == 0 && hint_device == Device::Pad);
}

} // namespace lastdevice
