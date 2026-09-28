#pragma once

#include <cstddef>
#include <span>

namespace canary {

class Balance {
public:
    void setDecibels(float decibels);
    void snapToTarget();
    float leftLinear() const;
    float rightLinear() const;

    void process(std::span<float> samples, std::size_t channels);

private:
    float targetLeft_ = 1.0f;
    float targetRight_ = 1.0f;
    float currentLeft_ = 1.0f;
    float currentRight_ = 1.0f;
};

}
