// S17: the CPU address space of the Midway Pac-Man board. No rendering, no sound output, no timing.
// Every rule cites its row in docs/notes/hardware_facts.md (section 2 = memory map, section 3 = latch, section 6 = inputs)
// and the MAME address map pacman_map() (pacman.cpp) that the notes were cross-checked against.
#pragma once
#include <cstdint>
#include <vector>

enum class Region : uint8_t {
    None,         // nothing happens (unmapped, or a write to ROM / a nop-write)
    Rom,          // 0000-3FFF program ROM (read only)
    VideoRam,     // 4000-43FF tile codes
    ColorRam,     // 4400-47FF tile palette numbers
    Nop,          // 4800-4BFF: reads give 0xBF, writes ignored
    WorkRam,      // 4C00-4FEF
    SpriteRam,    // 4FF0-4FFF
    Latch,        // 5000-5007 write: 8-bit output latch
    Sound,        // 5040-505F write: sound registers
    SpriteCoords, // 5060-506F write: sprite coordinates
    Watchdog,     // 50C0 write
    In0,
    In1,
    Dsw1,
    Dsw2 // reads of the input ports
};

struct Decoded {
    Region region;
    uint16_t offset;
};
struct IoWrite {
    uint32_t frame;
    uint16_t address;
    uint8_t value;
}; // address as the CPU wrote it, before mirroring

class Bus {
public:
    Bus();
    // Decoding only (no side effects). offset is the index inside the region (for Latch the latch bit number 0-7).
    static Decoded decode_read(uint16_t addr);
    static Decoded decode_write(uint16_t addr);

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);
    // Z80 I/O ports: IN returns 0xFF; OUT to port 0 (low 8 bits) sets the interrupt vector byte (hardware_facts section 1).
    uint8_t port_in(uint16_t port) const;
    void port_out(uint16_t port, uint8_t value);

    bool load_program(const std::vector<uint8_t> &program); // exactly 16384 bytes
    void set_frame(uint32_t f) { frame = f; }               // stamped on I/O log entries

    // Input port values, active low: idle IN0 = IN1 = 0xFF; DSW1 factory default 0xC9 (hardware_facts section 6); DSW2 unused.
    uint8_t in0 = 0xFF, in1 = 0xFF, dsw1 = 0xC9, dsw2 = 0xFF;

    // State written by the program, for the later video, sound and interrupt steps.
    uint8_t rom[0x4000], videoram[0x400], colorram[0x400], workram[0x3F0], spriteram[0x10];
    uint8_t sprite_coords[0x10], sound_regs[0x20], latch[8];
    uint8_t irq_vector = 0;
    uint32_t watchdog_writes = 0;
    std::vector<IoWrite> io_log;                        // every write to 5000-5FFF (and its mirrors), in order
    bool irq_enabled() const { return latch[0] & 1; }   // 5000
    bool sound_enabled() const { return latch[1] & 1; } // 5001
    bool flip_screen() const { return latch[3] & 1; }   // 5003
    bool coin_counter() const { return latch[7] & 1; }  // 5007
private:
    uint32_t frame = 0;
};
