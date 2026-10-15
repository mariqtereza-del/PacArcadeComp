// wsg.cpp: the 3-voice Namco sound generator that turns register values into samples (see wsg.h).
#include "wsg.h"
#include <cstring>

Wsg::Wsg(const uint8_t *waveform_prom) { std::memcpy(prom_, waveform_prom, 256); }

void Wsg::recompute(int ch) {
    Voice &v = v_[ch];
    v.wave = regs_[ch * 5 + 5] & 7;
    v.vol = regs_[ch * 5 + 0x15] & 15;
    uint32_t f = (ch == 0) ? regs_[0x10] : 0;
    f += (uint32_t)regs_[ch * 5 + 0x11] << 4;
    f += (uint32_t)regs_[ch * 5 + 0x12] << 8;
    f += (uint32_t)regs_[ch * 5 + 0x13] << 12;
    f += (uint32_t)regs_[ch * 5 + 0x14] << 16;
    v.freq = f;
}

void Wsg::write(unsigned reg, uint8_t value) {
    reg &= 0x1F;
    regs_[reg] = value & 0x0F;
    if (reg >= 0x10 || reg == 5 || reg == 0x0A || reg == 0x0F)
        for (int c = 0; c < 3; ++c) recompute(c);
}

void Wsg::load_registers(const uint8_t regs[32]) {
    for (unsigned i = 0; i < 32; ++i) write(i, regs[i]);
}

void Wsg::render(int16_t *out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (!enabled_) {
            out[i] = 0;
            continue;
        }
        int sum = 0;
        for (int c = 0; c < 3; ++c) {
            Voice &v = v_[c];
            if (mask_ & (1u << c)) sum += waveform_sample(prom_, v.wave, (int)(v.acc >> 15)) * v.vol;
            v.acc = (v.acc + v.freq) & 0xFFFFF; // 20-bit accumulator, advanced after the sample is read
        }
        out[i] = (int16_t)(sum * 32767 / 384);
    }
}
