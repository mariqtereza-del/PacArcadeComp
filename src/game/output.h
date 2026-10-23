// N53: the named OUTPUT state of the game: what the native game hands to the screen, the sprite hardware and the sound chip. Written by hand (no ROM bytes).
// Same bytes as the machine's arrays (src/board/bus.h), so the renderer, the sound chip and the lockstep harness see exactly what they saw before.
//   screen   : tile codes 4000-43FF and tile colours 4400-47FF (colour = low 5 bits = palette number)
//   sprites  : 8 slots at 4FF0 {shape byte: bits 2-7 picture number, bit 1 flip up/down, bit 0 flip left/right ; palette byte: low 5 bits}
//   coords   : 8 slots at 5060 {y byte (picture y = byte - 31), x byte (picture x = 272 - byte)}
//   sound    : the 32 registers of the sound chip at 5040-505F (docs/notes/hardware_facts.md section 5)
//   latches  : the 8 output latches at 5000-5007
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace game {
constexpr int kScreenTiles = 0x400;
struct SpriteSlot { uint8_t shape; uint8_t palette; };                 // 4FF0 + 2n
struct SpriteList { SpriteSlot slot[8]; };
struct SpriteCoord { uint8_t y; uint8_t x; };                           // 5060 + 2n
struct SpriteCoords { SpriteCoord slot[8]; };
struct SoundRegs {   // 5040-505F; waveform select sits in the three "accumulator" gaps (reg v*5+5), volume at v*5+0x15
    uint8_t accumulator0[5];  uint8_t waveform0;   // 5040-5044, 5045
    uint8_t accumulator1[4];  uint8_t waveform1;   // 5046-5049, 504A
    uint8_t accumulator2[4];  uint8_t waveform2;   // 504B-504E, 504F
    uint8_t frequency0[5];    uint8_t volume0;     // 5050-5054 (5 nibbles), 5055
    uint8_t frequency1[4];    uint8_t volume1;     // 5056-5059 (4 nibbles), 505A
    uint8_t frequency2[4];    uint8_t volume2;     // 505B-505E (4 nibbles), 505F
};
struct Latches {
    uint8_t interrupt_enable, sound_enable, aux_enable, flip_screen, lamp_player1, lamp_player2, coin_lockout, coin_counter;   // 5000-5007
};
static_assert(sizeof(SpriteSlot) == 2 && sizeof(SpriteList) == 16 && sizeof(SpriteCoord) == 2 && sizeof(SpriteCoords) == 16, "sprite structs");
static_assert(sizeof(SoundRegs) == 32 && sizeof(Latches) == 8, "sound and latch structs");
static_assert(offsetof(SoundRegs, waveform0) == 5 && offsetof(SoundRegs, waveform1) == 0xA && offsetof(SoundRegs, waveform2) == 0xF, "waveform registers v*5+5");
static_assert(offsetof(SoundRegs, frequency0) == 0x10 && offsetof(SoundRegs, volume0) == 0x15 && offsetof(SoundRegs, frequency1) == 0x16 && offsetof(SoundRegs, volume1) == 0x1A &&
              offsetof(SoundRegs, frequency2) == 0x1B && offsetof(SoundRegs, volume2) == 0x1F, "frequency and volume registers");
static_assert(offsetof(Latches, sound_enable) == 1 && offsetof(Latches, flip_screen) == 3 && offsetof(Latches, coin_counter) == 7, "latch anchors");

struct Screen { uint8_t (&tile)[kScreenTiles]; uint8_t (&colour)[kScreenTiles]; };   // references into the machine's video and colour RAM
// All the output of the game, as named parts. Make one from anything with the arrays of the Bus (Bus, or a test's copy).
struct Output { Screen screen; SpriteList &sprites; SpriteCoords &coords; SoundRegs &sound; Latches &latches; };
template <class B> inline Output output_view(B &b) {
    return Output{Screen{b.videoram, b.colorram}, *reinterpret_cast<SpriteList *>(b.spriteram), *reinterpret_cast<SpriteCoords *>(b.sprite_coords),
                  *reinterpret_cast<SoundRegs *>(b.sound_regs), *reinterpret_cast<Latches *>(b.latch)};
}
// The picture of one sprite slot, decoded from the named bytes (the formula of src/video/sprites.cpp).
struct SpritePicture { int number; bool flip_y, flip_x; int palette; int x, y; };
inline SpritePicture sprite_picture(const Output &o, int n) {
    const SpriteSlot &s = o.sprites.slot[n]; const SpriteCoord &c = o.coords.slot[n];
    return SpritePicture{s.shape >> 2, (s.shape & 2) != 0, (s.shape & 1) != 0, s.palette & 0x1F, 272 - c.x, c.y - 31};
}
// Chip values from the named registers (nibbles): 20-bit frequency, 4-bit volume, 3-bit waveform.
inline uint32_t sound_frequency(const SoundRegs &r, int v) {
    const uint8_t *f = v == 0 ? r.frequency0 : v == 1 ? r.frequency1 : r.frequency2;
    uint32_t hz = 0;
    if (v == 0) { for (int i = 0; i < 5; ++i) hz |= (uint32_t)(f[i] & 15) << (4 * i); }
    else { for (int i = 0; i < 4; ++i) hz |= (uint32_t)(f[i] & 15) << (4 * (i + 1)); }
    return hz;
}
inline int sound_volume(const SoundRegs &r, int v) { return (v == 0 ? r.volume0 : v == 1 ? r.volume1 : r.volume2) & 15; }
inline int sound_waveform(const SoundRegs &r, int v) { return (v == 0 ? r.waveform0 : v == 1 ? r.waveform1 : r.waveform2) & 7; }
// The OLD layout (the machine's separate arrays) rebuilt from the named output: the lockstep harness compares with this.
inline void output_to_old_layout(const Output &o, uint8_t *video, uint8_t *colour, uint8_t *spriteram, uint8_t *coords, uint8_t *sound, uint8_t *latch) {
    std::memcpy(video, o.screen.tile, kScreenTiles); std::memcpy(colour, o.screen.colour, kScreenTiles);
    std::memcpy(spriteram, &o.sprites, 16); std::memcpy(coords, &o.coords, 16); std::memcpy(sound, &o.sound, 32); std::memcpy(latch, &o.latches, 8);
}
} // namespace game
