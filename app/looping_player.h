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
    LoopingPlayer(std::vector<float> samples, double sampleRate, std::size_t channels, float initialGainDecibels);

    void setGainDecibels(float decibels);
    void setBalanceDecibels(float decibels);
    void setEqDecibels(Band band, float decibels);
    void setPaused(bool paused);
    void seekSeconds(double seconds);

    bool paused() const;
    double positionSeconds() const;
    double durationSeconds() const;

    void render(std::span<float> buffer, std::uint32_t channels) override;

private:
    void copyNextSamples(std::span<float> buffer);

    std::vector<float> samples_;
    double sampleRate_;
    std::size_t channels_;
    std::size_t position_ = 0;

    std::atomic<float> gainDecibels_;
    std::atomic<float> balanceDecibels_{0.0f};
    std::array<std::atomic<float>, ThreeBandEq::kBandCount> eqDecibels_{};
    std::atomic<bool> paused_{false};
    std::atomic<std::int64_t> seekRequestFrame_{-1};
    std::atomic<std::uint64_t> positionFrame_{0};

    Gain gain_;
    Balance balance_;
    ThreeBandEq eq_;
    Gain transport_;
};

}
