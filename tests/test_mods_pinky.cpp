// T02j: probe for the worked C++ example of MODDING.md (Pinky aims further ahead). It prints Pac-Man's tile, the step he is moving in and Pinky's target in the chase phase.
// The test script compiles this file twice: against src/logic and against a copy in which `pac_step * 4` was changed to `pac_step * 6`.
#include <cstdio>
#include <cstring>
#include "ghost_targeting.h"
int main() {
    uint8_t work[logic::kWorkRamSize];
    std::memset(work, 0, sizeof work);
    uint8_t video[logic::kVideoRamSize];
    std::memset(video, 0x40, sizeof video);
    logic::WorkRam ram{work};
    logic::VideoRam vr{video};
    ram[logic::kPhaseAddress] = 1;                         // chase
    ram[logic::kLevelStateAddress] = logic::kLevelStateRunning;
    ram[logic::kPacmanTileAddress] = 100;                  // Pac-Man's tile (a, b) = (100, 100)
    ram[logic::kPacmanTileAddress + 1] = 100;
    const unsigned dir = 1;                                // moving "down": the step is (da, db) = (1, 0)
    const uint16_t sw = logic::step_word(dir);
    ram[logic::kPacmanVectorAddress] = sw & 255;
    ram[logic::kPacmanVectorAddress + 1] = sw >> 8;
    ram[logic::kGhostTileAddress[1]] = 60;
    ram[logic::kGhostTileAddress[1] + 1] = 60;
    logic::TaskReport rep;
    logic::pinky_target_task(ram, vr, &rep);
    std::printf("%u %u\n", (unsigned)rep.target.a, (unsigned)rep.target.b);
    return 0;
}
