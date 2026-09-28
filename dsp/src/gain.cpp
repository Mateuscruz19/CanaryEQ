#include "canary/gain.h"

#include <cmath>

namespace canary {

void Gain::setLinear(float gain)
{
    target_ = gain;
}

void Gain::setDecibels(float decibels)
{
    target_ = std::pow(10.0f, decibels / 20.0f);
}

void Gain::snapToTarget()
{
    current_ = target_;
}

float Gain::linear() const
{
    return target_;
}

void Gain::process(std::span<float> samples, std::size_t channels)
{
    std::size_t frames = channels == 0 ? 0 : samples.size() / channels;
    if (frames == 0) {
        return;
    }

    if (current_ == target_) {
        for (float& sample : samples) {
            sample *= current_;
        }
        return;
    }

    float step = (target_ - current_) / static_cast<float>(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float gain = current_ + step * static_cast<float>(frame + 1);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            samples[frame * channels + channel] *= gain;
        }
    }
    current_ = target_;
}

}
