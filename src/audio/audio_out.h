// S26: the pieces between the sound chip (96 kHz, made once per frame on the emulation side) and the sound card (its own rate, pulled by a callback
// on another thread): a lock-free ring buffer, an exact area-averaging resampler, and a small controller that keeps the ring level steady.
// No SDL in here, so it can be tested without a sound card.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Single producer (the emulation thread), single consumer (the audio callback). Capacity must be a power of two.
class AudioRing {
public:
    explicit AudioRing(size_t capacity_pow2) : buf_(capacity_pow2), mask_(capacity_pow2 - 1) {}
    size_t write(const int16_t *src, size_t n); // returns how many fitted (the rest is dropped and counted by the caller)
    size_t read(int16_t *dst, size_t n);        // returns how many were available
    size_t size() const { return w_.load(std::memory_order_acquire) - r_.load(std::memory_order_acquire); }
    size_t capacity() const { return buf_.size(); }

private:
    std::vector<int16_t> buf_;
    size_t mask_;
    std::atomic<size_t> w_{0}, r_{0};
};

// Each output sample is the exact average of the input over the span it covers (a box filter: no gaps, no repeats, aliasing kept low when going down
// from 96 kHz). The span length can be nudged a little at a time (set_scale) so the long-term rate follows the sound card's real clock.
class AreaResampler {
public:
    AreaResampler(double in_rate, double out_rate);
    void set_scale(double s);                                             // 1.0 = nominal; > 1 makes fewer output samples (each covers more input)
    void process(const int16_t *in, size_t n, std::vector<int16_t> &out); // appends to out
private:
    double nominal_, step_, acc_ = 0, need_;
};

// How much to nudge the resampler so that the ring level, measured just before each new block is added, settles at the target.
// Returns the scale for AreaResampler::set_scale: above target -> a little above 1 (drain faster), below target -> a little below 1. Limited to +/- 2.5 % (a full error of the target level gives 2.5 %). Feed it a smoothed level, never the raw sawtooth, or the pitch would wobble.
double ring_level_scale(size_t level, size_t target);

// S32 fix (user report: unplugging the pad raised the pitch of the sound for a few seconds). When the program freezes for a moment (Windows can take
// 100 ms or more to remove a game pad) the sound card plays on and the queue runs empty; then the machine catches up with a burst of frames and the
// queue is too full. The level controller above then pushed the pitch by up to 2.5 % for about 3 s to correct it. RingGuard removes the cause: it is
// asked before every block is queued. After the card ran dry it asks for a little silence so the queue starts at the target level, and it refuses a block
// that is made while the machine is catching up and the queue already holds its normal amount (the sound of the frames skipped while frozen is not played late). So the controller never
// sees a big error and the pitch stays where it belongs. Pure logic, no SDL.
struct GuardDecision {
    size_t pad_zeros;
    bool write;
    bool resync;
}; // resync: the caller should reset its smoothed level to the real level
class RingGuard {
public:
    GuardDecision decide(
        size_t level, uint64_t underruns, size_t target, bool catching_up,
        size_t high =
            0); // high (0 = off): S36: when the queue is already longer than this, the new block of sound is dropped (a short click instead of a long delay); catching_up: the machine is running frames back to back to make up lost time
private:
    uint64_t seen_underruns_ = 0;
};

struct AudioStats {
    std::atomic<uint64_t> callbacks{0}, underruns{0}, missing_samples{0}, dropped_samples{0};
    std::atomic<uint64_t> level_sum{0}, level_max{0}, level_min{~0ull}; // ring level (samples) seen at the start of each callback after playback began
    std::atomic<uint64_t> level_count{0};
};
