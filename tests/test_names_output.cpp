// N53: the named output state (src/game/output.h). In every frame of the S27 replay the named screen, sprite, coordinate, sound and latch structs are the same bytes as the
// machine's arrays; the sprite picture decoded from the names matches the formula of the renderer; the chip values decoded from the names match the sound chip (Wsg); the
// old layout rebuilt from the names equals the machine; a view shifted by one byte is caught.
#include <cstdio>
#include <cstring>
#include <vector>
#include "board/machine.h"
#include "board/romset.h"
#include "audio/wsg.h"
#include "audio/wsg.cpp"
#include "game/output.h"
using namespace game;
static int bad = 0;
static void chk(bool ok, const char *what) { if (!ok) { ++bad; std::printf("  FAIL: %s\n", what); } }
int main(int argc, char **argv) {
    if (argc < 3) { std::printf("usage: test_names_output PROG.BIN REPLAY\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::printf("cannot read ROM\n"); return 2; }
    std::fclose(f);
    std::vector<unsigned> fr, a0, a1; unsigned dip = 0xC9; long nfr = 0; char line[256];
    FILE *rf = std::fopen(argv[2], "r");
    if (!rf) { std::printf("cannot read replay\n"); return 2; }
    while (std::fgets(line, sizeof line, rf)) { unsigned a, b, c; if (!std::strncmp(line, "dip ", 4)) dip = (unsigned)std::strtoul(line + 4, nullptr, 16); else if (!std::strncmp(line, "frames ", 7)) nfr = std::atol(line + 7); else if (line[0] != '#' && std::sscanf(line, "%u %x %x", &a, &b, &c) == 3) { fr.push_back(a); a0.push_back(b); a1.push_back(c); } }
    std::fclose(rf);
    static Machine m(prog);
    static uint8_t wprom[256]; for (int i = 0; i < 256; ++i) wprom[i] = (uint8_t)((i * 7 + 3) & 15);   // a stand-in waveform table: only the register decoding is tested here
    Wsg w(wprom);
    size_t k = 0; unsigned in0 = 0xFF, in1 = 0xFF; long compared = 0, sprites_seen = 0, sound_seen = 0, flipped = 0; 
    static uint8_t ov[0x400], oc[0x400], os[16], oq[16], ow[32], ol[8];
    for (long i = 0; i < nfr && bad == 0; ++i) {
        while (k < fr.size() && fr[k] <= (unsigned)i) { in0 = a0[k]; in1 = a1[k]; ++k; }
        m.bus.in0 = (uint8_t)in0; m.bus.in1 = (uint8_t)in1; m.bus.dsw1 = (uint8_t)dip;
        m.run_frame();
        Output o = output_view(m.bus);
        for (int a = 0; a < 0x400; ++a) if (o.screen.tile[a] != m.bus.read((uint16_t)(0x4000 + a)) || o.screen.colour[a] != m.bus.read((uint16_t)(0x4400 + a))) { chk(false, "screen differs from memory"); break; }
        for (int a = 0; a < 16; ++a) if (((uint8_t *)&o.sprites)[a] != m.bus.read((uint16_t)(0x4FF0 + a))) { chk(false, "sprite list differs from memory"); break; }
        for (int a = 0; a < 32; ++a) if (((uint8_t *)&o.sound)[a] != m.bus.sound_regs[a]) { chk(false, "sound registers differ"); break; }
        for (int a = 0; a < 8; ++a) if (((uint8_t *)&o.latches)[a] != m.bus.latch[a]) { chk(false, "latches differ"); break; }
        chk(o.latches.sound_enable == m.bus.latch[1] && (o.latches.flip_screen & 1) == (m.bus.flip_screen() ? 1 : 0) && (o.latches.interrupt_enable & 1) == (m.bus.irq_enabled() ? 1 : 0), "named latches");
        for (int n = 0; n < 8; ++n) {   // the formula of src/video/sprites.cpp, spelled out on the raw arrays
            SpritePicture p = sprite_picture(o, n);
            chk(p.number == (m.bus.spriteram[2 * n] >> 2) && p.flip_x == !!(m.bus.spriteram[2 * n] & 1) && p.flip_y == !!(m.bus.spriteram[2 * n] & 2) && p.palette == (m.bus.spriteram[2 * n + 1] & 0x1F) &&
                p.x == 272 - m.bus.sprite_coords[2 * n + 1] && p.y == m.bus.sprite_coords[2 * n] - 31, "sprite picture decoding");
            if (p.number != 0 || m.bus.sprite_coords[2 * n]) ++sprites_seen;
        }
        w.load_registers((const uint8_t *)&o.sound);
        for (int v = 0; v < 3; ++v) {
            chk(sound_frequency(o.sound, v) == w.frequency(v) && sound_volume(o.sound, v) == w.volume(v) && sound_waveform(o.sound, v) == w.waveform(v), "chip values from the names");
            if (w.volume(v) && w.frequency(v)) ++sound_seen;
        }
        output_to_old_layout(o, ov, oc, os, oq, ow, ol);
        chk(!std::memcmp(ov, m.bus.videoram, 0x400) && !std::memcmp(oc, m.bus.colorram, 0x400) && !std::memcmp(os, m.bus.spriteram, 16) && !std::memcmp(oq, m.bus.sprite_coords, 16) &&
            !std::memcmp(ow, m.bus.sound_regs, 32) && !std::memcmp(ol, m.bus.latch, 8), "old layout rebuilt from the names");
        if (o.latches.flip_screen & 1) ++flipped;
        ++compared;
    }
    // a view one byte off must be caught: the sprite list and the sound registers read one byte late no longer equal the memory
    { const uint8_t *sh = (const uint8_t *)&output_view(m.bus).sprites + 1; bool caught = false; for (int a = 0; a < 15; ++a) if (sh[a] != m.bus.spriteram[a]) caught = true; chk(caught, "a shifted sprite view was NOT caught"); }
    { const uint8_t *sh = (const uint8_t *)&output_view(m.bus).sound + 1; bool caught = false; for (int a = 0; a < 31; ++a) if (sh[a] != m.bus.sound_regs[a]) caught = true; chk(caught, "a shifted sound view was NOT caught"); }
    std::printf("%ld frames compared (screen 2 x 1024 bytes, 8 sprite slots, 8 coordinate slots, 32 sound registers, 8 latches); sprite slots in use: %ld; voices sounding: %ld\n", compared, sprites_seen, sound_seen);
    chk(sprites_seen > 1000 && sound_seen > 500, "the replay really showed sprites and made sound");
    std::printf("%s test_names_output\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
