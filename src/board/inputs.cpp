// inputs.cpp: the board's input ports and DIP switches (see inputs.h).
#include "inputs.h"

namespace {
struct Wire {
    int port;
    int bit;
}; // port 0 = IN0, 1 = IN1
const Wire wires[(int)Control::Count] = {
    {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7},
};
// Order above must follow enum Control: P1Up..P1Down, RackTest, Coin1, Coin2, ServiceCoin, P2Up..P2Down, ServiceMode, Start1, Start2, Cocktail.
} // namespace

uint8_t DipSettings::to_byte() const {
    return (uint8_t)((int)coinage | (int)lives << 2 | (int)bonus << 4 | (normal_difficulty ? 0x40 : 0) | (normal_ghost_names ? 0x80 : 0));
}

DipSettings DipSettings::from_byte(uint8_t v) {
    DipSettings d;
    d.coinage = (Coinage)(v & 3);
    d.lives = (Lives)((v >> 2) & 3);
    d.bonus = (Bonus)((v >> 4) & 3);
    d.normal_difficulty = (v & 0x40) != 0;
    d.normal_ghost_names = (v & 0x80) != 0;
    return d;
}

Inputs::Inputs() {}

void Inputs::set(Control c, bool on) {
    const Wire &w = wires[(int)c];
    uint8_t &port = w.port == 0 ? in0_ : in1_;
    uint8_t mask = (uint8_t)(1u << w.bit);
    if (c == Control::Cocktail) { cocktail_ = on ? 1 : 0; }
    if (on) port &= (uint8_t)~mask;
    else port |= mask; // active low: pressed = 0
}

bool Inputs::pressed(Control c) const {
    const Wire &w = wires[(int)c];
    return ((w.port == 0 ? in0_ : in1_) & (1u << w.bit)) == 0;
}

void Inputs::release_all() {
    in0_ = 0xFF;
    in1_ = cocktail_ ? 0x7F : 0xFF;
}

void Inputs::apply(Bus &bus) const {
    bus.in0 = in0_;
    bus.in1 = in1_;
    bus.dsw1 = dsw1();
}
