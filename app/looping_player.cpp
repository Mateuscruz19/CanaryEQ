#include "looping_player.h"

#include <algorithm>
#include <utility>

namespace canary {

static_assert(std::atomic<float>::is_always_lock_free);

LoopingPlayer::LoopingPlayer(std::vector<float> samples, double sampleRate, float initialGainDecibels)
    : samples_(std::move(samples))
    , gainDecibels_(initialGainDecibels)
    , eq_(sampleRate)
{
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

void LoopingPlayer::render(std::span<float> buffer, std::uint32_t channels)
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

    for (std::size_t band = 0; band < ThreeBandEq::kBandCount; ++band) {
        eq_.setGainDecibels(static_cast<Band>(band), eqDecibels_[band].load(std::memory_order_relaxed));
    }
    gain_.setDecibels(gainDecibels_.load(std::memory_order_relaxed));
    balance_.setDecibels(balanceDecibels_.load(std::memory_order_relaxed));

    eq_.process(buffer, channels);
    balance_.process(buffer, channels);
    gain_.process(buffer, channels);
}

}
