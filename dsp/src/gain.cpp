#include "canary/gain.h"

namespace canary {

void Gain::setLinear(float gain)
{
    gain_ = gain;
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
