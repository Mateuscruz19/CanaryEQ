#include "looping_player.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace canary {

static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::int64_t>::is_always_lock_free);

LoopingPlayer::LoopingPlayer(std::vector<float> samples, double sampleRate, std::size_t channels,
                             float initialGainDecibels)
    : samples_(std::move(samples))
    , sampleRate_(sampleRate)
    , channels_(channels == 0 ? 1 : channels)
    , gainDecibels_(initialGainDecibels)
    , eq_(sampleRate)
{
    samples_.resize(samples_.size() - samples_.size() % channels_);
    gain_.setDecibels(initialGainDecibels);
    gain_.snapToTarget();
}

void LoopingPlayer::setGainDecibels(float decibels)
{
    gainDecibels_.store(decibels, std::memory_order_relaxed);
}

void LoopingPlayer::setBalanceDecibels(float decibels)
{
    balanceDecibels_.store(decibels, std::memory_order_relaxed);
}

void LoopingPlayer::setEqDecibels(Band band, float decibels)
{
    eqDecibels_[static_cast<std::size_t>(band)].store(decibels, std::memory_order_relaxed);
}

void LoopingPlayer::setPaused(bool paused)
{
    paused_.store(paused, std::memory_order_relaxed);
}

void LoopingPlayer::seekSeconds(double seconds)
{
    double frame = std::clamp(seconds * sampleRate_, 0.0, durationSeconds() * sampleRate_);
    seekRequestFrame_.store(static_cast<std::int64_t>(frame), std::memory_order_relaxed);
}

bool LoopingPlayer::paused() const
{
    return paused_.load(std::memory_order_relaxed);
}

double LoopingPlayer::positionSeconds() const
{
    return static_cast<double>(positionFrame_.load(std::memory_order_relaxed)) / sampleRate_;
}

double LoopingPlayer::durationSeconds() const
{
    return static_cast<double>(samples_.size() / channels_) / sampleRate_;
}

void LoopingPlayer::render(std::span<float> buffer, std::uint32_t channels)
{
    bool paused = paused_.load(std::memory_order_relaxed);
    transport_.setLinear(paused ? 0.0f : 1.0f);

    if (samples_.empty() || (paused && transport_.currentLinear() == 0.0f)) {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        return;
    }

    std::int64_t seek = seekRequestFrame_.exchange(-1, std::memory_order_relaxed);
    if (seek >= 0) {
        position_ = std::min(static_cast<std::size_t>(seek) * channels_, samples_.size() - channels_);
    }

    copyNextSamples(buffer);
    positionFrame_.store(position_ / channels_, std::memory_order_relaxed);

    for (std::size_t band = 0; band < ThreeBandEq::kBandCount; ++band) {
        eq_.setGainDecibels(static_cast<Band>(band), eqDecibels_[band].load(std::memory_order_relaxed));
    }
    gain_.setDecibels(gainDecibels_.load(std::memory_order_relaxed));
    balance_.setDecibels(balanceDecibels_.load(std::memory_order_relaxed));

    eq_.process(buffer, channels);
    balance_.process(buffer, channels);
    gain_.process(buffer, channels);
    transport_.process(buffer, channels);
}

void LoopingPlayer::copyNextSamples(std::span<float> buffer)
{
    std::size_t written = 0;
    while (written < buffer.size()) {
        std::size_t count = std::min(buffer.size() - written, samples_.size() - position_);
        std::copy_n(samples_.data() + position_, count, buffer.data() + written);
        written += count;
        position_ += count;
        if (position_ == samples_.size()) {
            position_ = 0;
        }
    }
}

}
