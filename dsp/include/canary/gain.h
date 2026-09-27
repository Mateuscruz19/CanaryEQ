#pragma once

#include <span>

namespace canary {

class Gain {
public:
    void setLinear(float gain);
    float linear() const;

    float processSample(float sample) const;
    void process(std::span<float> samples) const;

private:
    float gain_ = 1.0f;
};

}
