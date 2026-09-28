#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "canary/audio_output.h"
#include "canary/balance.h"
#include "canary/gain.h"

namespace canary {

class LoopingPlayer final : public AudioRenderer {
public:
    LoopingPlayer(std::vector<float> samples, float initialGainDecibels);

    void setGainDecibels(float decibels);
    void setBalanceDecibels(float decibels);
    void render(std::span<float> buffer, std::uint32_t channels) override;

private:
    std::vector<float> samples_;
    std::size_t position_ = 0;
    std::atomic<float> gainDecibels_;
    std::atomic<float> balanceDecibels_{0.0f};
    Gain gain_;
    Balance balance_;
};

}
