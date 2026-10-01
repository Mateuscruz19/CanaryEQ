#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "canary/audio_output.h"
#include "canary/balance.h"
#include "canary/gain.h"
#include "canary/three_band_eq.h"

namespace canary {

class LoopingPlayer final : public AudioRenderer {
public:
    LoopingPlayer(std::vector<float> samples, double sampleRate, float initialGainDecibels);

    void setGainDecibels(float decibels);
    void setBalanceDecibels(float decibels);
    void setEqDecibels(Band band, float decibels);
    void render(std::span<float> buffer, std::uint32_t channels) override;

private:
    std::vector<float> samples_;
    std::size_t position_ = 0;
    std::atomic<float> gainDecibels_;
    std::atomic<float> balanceDecibels_{0.0f};
    std::array<std::atomic<float>, ThreeBandEq::kBandCount> eqDecibels_{};
    Gain gain_;
    Balance balance_;
    ThreeBandEq eq_;
};

}
