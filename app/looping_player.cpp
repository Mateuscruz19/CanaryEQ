#include "looping_player.h"

#include <algorithm>
#include <utility>

namespace canary {

LoopingPlayer::LoopingPlayer(std::vector<float> samples)
    : samples_(std::move(samples))
{
}

void LoopingPlayer::setGainDecibels(float decibels)
{
    gain_.setDecibels(decibels);
}

void LoopingPlayer::render(std::span<float> buffer, std::uint32_t)
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

    gain_.process(buffer);
}

}
