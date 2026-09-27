#include "canary/gain.h"

#include <cmath>

namespace canary {

void Gain::setLinear(float gain)
{
    gain_ = gain;
}

void Gain::setDecibels(float decibels)
{
    gain_ = std::pow(10.0f, decibels / 20.0f);
}

float Gain::linear() const
{
    return gain_;
}

float Gain::processSample(float sample) const
{
    return sample * gain_;
}

void Gain::process(std::span<float> samples) const
{
    for (float& sample : samples) {
        sample = processSample(sample);
    }
}

}
