// audio_out.cpp: ring buffer and resampler between the sound chip and the sound card (see audio_out.h).
#include "audio_out.h"
#include <algorithm>
#include <cmath>

size_t AudioRing::write(const int16_t *src, size_t n) {
    size_t w = w_.load(std::memory_order_relaxed), r = r_.load(std::memory_order_acquire);
    size_t room = buf_.size() - (w - r);
    if (n > room) n = room;
    for (size_t i = 0; i < n; ++i) buf_[(w + i) & mask_] = src[i];
    w_.store(w + n, std::memory_order_release);
    return n;
}

size_t AudioRing::read(int16_t *dst, size_t n) {
    size_t r = r_.load(std::memory_order_relaxed), w = w_.load(std::memory_order_acquire);
    size_t have = w - r;
    if (n > have) n = have;
    for (size_t i = 0; i < n; ++i) dst[i] = buf_[(r + i) & mask_];
    r_.store(r + n, std::memory_order_release);
    return n;
}

AreaResampler::AreaResampler(double in_rate, double out_rate) : nominal_(in_rate / out_rate), step_(nominal_), need_(nominal_) {}

void AreaResampler::set_scale(double s) { step_ = nominal_ * s; }

void AreaResampler::process(const int16_t *in, size_t n, std::vector<int16_t> &out) {
    for (size_t i = 0; i < n; ++i) {
        double w = 1.0;
        while (w > 1e-12) {
            double take = std::min(w, need_);
            acc_ += in[i] * take;
            w -= take;
            need_ -= take;
            if (need_ <= 1e-12) {
                double v = std::floor(acc_ / step_ + 0.5);
                out.push_back((int16_t)std::max(-32768.0, std::min(32767.0, v)));
                acc_ = 0;
                need_ = step_;
            }
        }
    }
}

double ring_level_scale(size_t level, size_t target) {
    if (target == 0) return 1.0;
    double e = 0.025 * ((double)level - (double)target) / (double)target;
    return 1.0 + std::max(-0.025, std::min(0.025, e));
}

GuardDecision RingGuard::decide(size_t level, uint64_t underruns, size_t target, bool catching_up, size_t high) {
    GuardDecision d{0, true, false};
    if (underruns != seen_underruns_) { // the card ran dry since the last block: start again from the target level
        seen_underruns_ = underruns;
        if (level < target) d.pad_zeros = target - level;
        d.resync = true;
    }
    if (catching_up && level + d.pad_zeros >= target) {
        d.write = false;
        d.resync = true;
    } // a catch-up burst: the queue is already at its normal level, drop this block
    if (high && level > high) {
        d.write = false;
        d.resync = true;
    } // S36: far too much sound queued (a burst of frames): drop this block so the delay cannot build up
    return d;
}
