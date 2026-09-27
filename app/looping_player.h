#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "canary/audio_output.h"
#include "canary/gain.h"

namespace canary {

class LoopingPlayer final : public AudioRenderer {
public:
    explicit LoopingPlayer(std::vector<float> samples);

    void setGainDecibels(float decibels);
    void render(std::span<float> buffer, std::uint32_t channels) override;

private:
    std::vector<float> samples_;
    std::size_t position_ = 0;
    Gain gain_;
};

}
