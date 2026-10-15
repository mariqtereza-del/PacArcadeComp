// bus.cpp: the CPU address space of the board: memory map, mirrors and I/O ports (see bus.h).
#include "bus.h"
#include <cstring>

Bus::Bus() {
    std::memset(rom, 0, sizeof rom);
    std::memset(videoram, 0, sizeof videoram);
    std::memset(colorram, 0, sizeof colorram);
    std::memset(workram, 0, sizeof workram);
    std::memset(spriteram, 0, sizeof spriteram);
    std::memset(sprite_coords, 0, sizeof sprite_coords);
    std::memset(sound_regs, 0, sizeof sound_regs);
    std::memset(latch, 0, sizeof latch);
}

// Decoding rules. Let hi = the address with bits 13 and 15 cleared when bit 14 is set.
//  - Bit 14 clear: ROM, 0000-3FFF and its copy at 8000-BFFF (hardware_facts section 2 row "0000-3FFF", MAME mirror 0x8000).
//  - Bit 14 set: bits 13 and 15 are ignored (MAME mirror 0xA000 on every RAM entry, 0xAF3F/0xAF38/0xAF00 on the I/O entries),
//    so the map 4000-5FFF repeats at 6000-7FFF, C000-DFFF and E000-FFFF.
//  - 4000-4FFF are memory, 5000-5FFF are I/O whose low 12 bits are decoded as below (bits 8-11 ignored by every I/O entry).
Decoded Bus::decode_read(uint16_t a) {
    if (!(a & 0x4000)) return {Region::Rom, (uint16_t)(a & 0x3FFF)};
    uint16_t h = a & 0x5FFF;
    if (h < 0x4400) return {Region::VideoRam, (uint16_t)(h - 0x4000)};  // row "4000-43FF"
    if (h < 0x4800) return {Region::ColorRam, (uint16_t)(h - 0x4400)};  // row "4400-47FF"
    if (h < 0x4C00) return {Region::Nop, (uint16_t)(h - 0x4800)};       // row "4800-4BFF": reads 0xBF
    if (h < 0x4FF0) return {Region::WorkRam, (uint16_t)(h - 0x4C00)};   // row "4C00-4FEF"
    if (h < 0x5000) return {Region::SpriteRam, (uint16_t)(h - 0x4FF0)}; // row "4FF0-4FFF"
    switch ((h >> 6) & 3) {                                             // low byte 00-3F, 40-7F, 80-BF, C0-FF
    case 0: return {Region::In0, 0};                                    // row "5000 read: IN0" (mirror 0xAF3F)
    case 1: return {Region::In1, 0};                                    // row "5040-507F read: IN1"
    case 2: return {Region::Dsw1, 0};                                   // row "5080-50BF read: DIP switch 1"
    default: return {Region::Dsw2, 0};                                  // row "50C0-50FF read: DIP switch 2"
    }
}

Decoded Bus::decode_write(uint16_t a) {
    if (!(a & 0x4000)) return {Region::None, 0}; // ROM is read only
    uint16_t h = a & 0x5FFF;
    if (h < 0x4400) return {Region::VideoRam, (uint16_t)(h - 0x4000)};
    if (h < 0x4800) return {Region::ColorRam, (uint16_t)(h - 0x4400)};
    if (h < 0x4C00) return {Region::None, 0}; // row "4800-4BFF": writes ignored
    if (h < 0x4FF0) return {Region::WorkRam, (uint16_t)(h - 0x4C00)};
    if (h < 0x5000) return {Region::SpriteRam, (uint16_t)(h - 0x4FF0)};
    uint8_t lo = h & 0xFF;
    if (lo < 0x40) return {Region::Latch, (uint16_t)(lo & 7)};           // row "5000-5007 write: latch", mirror 0xAF38
    if (lo < 0x60) return {Region::Sound, (uint16_t)(lo - 0x40)};        // row "5040-505F write: sound registers"
    if (lo < 0x70) return {Region::SpriteCoords, (uint16_t)(lo - 0x60)}; // row "5060-506F write: sprite coordinates"
    if (lo < 0xC0) return {Region::None, 0};                             // row "5070-507F, 5080 writes ignored" (5080-50BF by mirror)
    return {Region::Watchdog, 0};                                        // row "50C0 write: watchdog" (50C0-50FF by mirror)
}

uint8_t Bus::read(uint16_t a) {
    Decoded d = decode_read(a);
    switch (d.region) {
    case Region::Rom: return rom[d.offset];
    case Region::VideoRam: return videoram[d.offset];
    case Region::ColorRam: return colorram[d.offset];
    case Region::Nop: return 0xBF; // hardware_facts section 2: [M] pacman_read_nop; the prototype returned 0 (no effect, E14)
    case Region::WorkRam: return workram[d.offset];
    case Region::SpriteRam: return spriteram[d.offset];
    case Region::In0: return in0;
    case Region::In1: return in1;
    case Region::Dsw1: return dsw1;
    case Region::Dsw2: return dsw2;
    default: return 0xFF;
    }
}

void Bus::write(uint16_t a, uint8_t v) {
    Decoded d = decode_write(a);
    switch (d.region) {
    case Region::VideoRam: videoram[d.offset] = v; return;
    case Region::ColorRam: colorram[d.offset] = v; return;
    case Region::WorkRam: workram[d.offset] = v; return;
    case Region::SpriteRam: spriteram[d.offset] = v; return;
    default: break;
    }
    if ((a & 0x4000) && (a & 0x1000)) { // I/O area 5000-5FFF (and mirrors): log every write, then apply it
        io_log.push_back({frame, a, v});
        switch (d.region) {
        case Region::Latch: latch[d.offset] = v & 1; break; // only data bit 0 counts (section 3)
        case Region::Sound: sound_regs[d.offset] = v; break;
        case Region::SpriteCoords: sprite_coords[d.offset] = v; break;
        case Region::Watchdog: ++watchdog_writes; break;
        default: break;
        }
    }
}

uint8_t Bus::port_in(uint16_t) const { return 0xFF; }
void Bus::port_out(uint16_t port, uint8_t v) {
    if ((port & 0xFF) == 0) irq_vector = v;
}

bool Bus::load_program(const std::vector<uint8_t> &p) {
    if (p.size() != sizeof rom) return false;
    std::memcpy(rom, p.data(), sizeof rom);
    return true;
}
