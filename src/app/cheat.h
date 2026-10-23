// The INVINCIBLE test cheat (Esc > CHEATS > INVINCIBLE): Pac-Man passes through ghosts unless a power pellet has made them blue; blue ghosts are eaten as usual.
// The collision code at ROM 1763 (shared by the tile scan 171D and the pixel scan 1789) stores the number of the ghost that touched Pac-Man in 4DA4 and 4DA5 and only THEN looks whether that ghost is blue.
// While the cheat is on, those 23 bytes are replaced by the same code with the two steps swapped: a ghost that is not blue is simply ignored (nothing is stored, the game never notices the touch); a blue
// one is stored exactly as before and eaten as usual. The 23 original bytes are checked before they are replaced and put back when the cheat is switched off. In the readable-rules mode the native
// copy of that code (src/game/attract.h) takes the same shortcut while game::att::g_invincible is set. The cheat is an assist (assist.h, reason kCheatHelp: the game stops counting for records and
// statistics), it is never active in Purist mode or in scripted runs, and it is not saved in settings.cfg: every start of the program begins with it off.
#pragma once
#include <cstdint>
#include <cstring>

namespace cheat {

// No ROM bytes are kept here: the patched code is built from the 23 bytes the player's own ROM holds at 1763 (their order is changed, plus two NOPs) and turned back the same way.
// Layout of the original (o) and of the patched code (p):  o: [0] 78  [1..3] store A in 4DA4  [4..6] store A in 4DA5  [7] and a  [8] ret z  [9..18] blue-flag lookup  [19] clear A  [20..22] store in 4DA5
//                                                          p: [0] o0  [1] o7  [2] o8  [3..12] o9..o18  [13] o0  [14..16] o1..o3  [17] o19  [18..20] o4..o6  [21..22] NOP
inline bool looks_original(const uint8_t *o) { return o[0] == 0x78 && o[1] == 0x32 && o[2] == 0xA4 && o[3] == 0x4D && o[7] == 0xA7 && o[8] == 0xC8 && o[19] == 0xAF && o[20] == 0x32 && o[21] == 0xA5 && o[22] == 0x4D; }
inline bool looks_patched(const uint8_t *p) { return p[0] == 0x78 && p[1] == 0xA7 && p[2] == 0xC8 && p[13] == 0x78 && p[14] == 0x32 && p[15] == 0xA4 && p[16] == 0x4D && p[17] == 0xAF && p[21] == 0 && p[22] == 0; }
inline void to_patched(const uint8_t *o, uint8_t *p) {
    p[0] = o[0]; p[1] = o[7]; p[2] = o[8]; std::memcpy(p + 3, o + 9, 10); p[13] = o[0]; std::memcpy(p + 14, o + 1, 3); p[17] = o[19]; std::memcpy(p + 18, o + 4, 3); p[21] = 0; p[22] = 0;
}
inline void to_original(const uint8_t *p, uint8_t *o) {
    o[0] = p[0]; std::memcpy(o + 1, p + 14, 3); std::memcpy(o + 4, p + 18, 3); o[7] = p[1]; o[8] = p[2]; std::memcpy(o + 9, p + 3, 10); o[19] = p[17]; std::memcpy(o + 20, p + 18, 3);
}
// Idempotent: call every frame with the wanted state. Returns true when the patch is in place. A ROM that does not look as expected is left alone (returns false).
template <class Bus> bool set_invincible(Bus &bus, bool on) {
    uint8_t *p = bus.rom + 0x1763;
    uint8_t t[23];
    if (on && looks_original(p)) { to_patched(p, t); std::memcpy(p, t, 23); }
    else if (!on && looks_patched(p)) { to_original(p, t); std::memcpy(p, t, 23); }
    return looks_patched(p);
}

}  // namespace cheat
