// S25: the Namco WSG, the 3-voice sound generator of the Pac-Man board. No playback here: it turns register values into samples (S26 plays them).
//
// Facts (docs/notes/hardware_facts.md section 5, Lomont p14-16, MAME src/devices/sound/namco.cpp):
//  - The chip runs at CPU clock / 32 = 96,000 Hz. One output sample per tick, so one 60.606 Hz frame is exactly 1,584 samples (96000 * 50688 / 3072000).
//  - The CPU writes the chip at 5040-505F (register 0x00-0x1F here). Each register holds 4 bits; only the low nibble counts.
//  - Voice v (0-2): waveform select = register v*5+5 (low 3 bits); volume = register v*5+0x15; frequency = 20 bits made of nibbles:
//      voice 0: reg 0x10 + reg 0x11<<4 + reg 0x12<<8 + reg 0x13<<12 + reg 0x14<<16 (five nibbles),
//      voices 1 and 2: reg v*5+0x11<<4 + ... + reg v*5+0x14<<16 (four nibbles, so the lowest nibble is always 0).
//    Registers 0x00-0x0F (the accumulators, 5040-504F) are not modelled: writes are accepted and ignored, as in MAME.
//  - Every tick each voice's 20-bit accumulator grows by its frequency value. The top 5 bits of the accumulator pick one of 32 samples of the
//    selected waveform (82s126.1m: 8 waveforms x 32 samples, low nibble 0-15, taken as value - 8). So pitch = frequency * 96000 / 2^20 Hz
//    (A440 = 0x12C6 on voice 0, Lomont's note table).
//  - The sample is read first, then the accumulator is advanced.
//  - Voice output = (sample - 8) * volume (volume 0-15). The three voices are added and divided by 384 (= 128 * 3 voices, MAME's MIX_RES), giving
//    -0.94 .. +0.82 of full scale. Our 16-bit output = sum * 32767 / 384 (integer, truncated toward zero).
//  - If the sound-enable latch (5001) is 0 the chip outputs silence and the accumulators stand still (MAME returns before mixing).
// Open question U3 (mixing details) is settled here by following MAME; the sample-by-sample comparison with a second description is in tests/test_wsg.cpp.
#pragma once
#include <cstddef>
#include <cstdint>

class Wsg {
public:
    static const int kRate = 96000;             // samples per second
    static const int kSamplesPerFrame = 1584;   // one 60.606 Hz frame
    explicit Wsg(const uint8_t *waveform_prom); // 256 bytes: pacman's 82s126.1m (RomSet::sound1)

    void write(unsigned reg, uint8_t value);                 // reg 0-31 = CPU address 5040-505F
    void load_registers(const uint8_t regs[32]);             // write all 32 (e.g. Bus::sound_regs)
    void set_enabled(bool on) { enabled_ = on; }             // the 5001 latch bit
    void set_voice_mask(unsigned mask) { mask_ = mask & 7; } // bit v = voice v is mixed in (default 7); for tests and for isolating a voice

    uint32_t frequency(int voice) const { return v_[voice].freq; } // 20-bit register value
    int volume(int voice) const { return v_[voice].vol; }
    int waveform(int voice) const { return v_[voice].wave; }
    uint32_t accumulator(int voice) const { return v_[voice].acc; }
    double frequency_hz(int voice) const { return v_[voice].freq * (double)kRate / 1048576.0; }
    static int waveform_sample(const uint8_t *prom, int wave, int index) { return (prom[(wave & 7) * 32 + (index & 31)] & 0x0F) - 8; }

    void render(int16_t *out, size_t n); // next n samples (mono, 96 kHz)

private:
    struct Voice {
        uint32_t freq = 0, acc = 0;
        int vol = 0, wave = 0;
    };
    void recompute(int voice);
    uint8_t prom_[256];
    uint8_t regs_[32] = {0};
    Voice v_[3];
    bool enabled_ = false;
    unsigned mask_ = 7;
};
