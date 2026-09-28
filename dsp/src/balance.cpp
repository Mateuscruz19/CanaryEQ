#include "canary/balance.h"

#include <cmath>

namespace canary {

void Balance::setDecibels(float decibels)
{
    float cut = std::pow(10.0f, -std::fabs(decibels) / 20.0f);
    targetLeft_ = decibels > 0.0f ? cut : 1.0f;
    targetRight_ = decibels < 0.0f ? cut : 1.0f;
}

void Balance::snapToTarget()
{
    currentLeft_ = targetLeft_;
    currentRight_ = targetRight_;
}

float Balance::leftLinear() const
{
    return targetLeft_;
}

float Balance::rightLinear() const
{
    return targetRight_;
}

void Balance::process(std::span<float> samples, std::size_t channels)
{
    if (channels != 2) {
        return;
    }

    std::size_t frames = samples.size() / 2;
    if (frames == 0) {
        return;
    }

    float stepLeft = (targetLeft_ - currentLeft_) / static_cast<float>(frames);
    float stepRight = (targetRight_ - currentRight_) / static_cast<float>(frames);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float progress = static_cast<float>(frame + 1);
        samples[frame * 2] *= currentLeft_ + stepLeft * progress;
        samples[frame * 2 + 1] *= currentRight_ + stepRight * progress;
    }

    currentLeft_ = targetLeft_;
    currentRight_ = targetRight_;
}

}
