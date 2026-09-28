#pragma once

#include <cstddef>
#include <span>

namespace canary {

class Gain {
public:
    void setLinear(float gain);
    void setDecibels(float decibels);
    void snapToTarget();
    float linear() const;

    void process(std::span<float> samples, std::size_t channels);

private:
    float target_ = 1.0f;
    float current_ = 1.0f;
};

}
