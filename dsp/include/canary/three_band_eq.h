#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "canary/biquad.h"
#include "canary/gain.h"

namespace canary {

enum class Band : std::size_t {
    Bass,
    Mid,
    Treble,
};

class ThreeBandEq {
public:
    static constexpr std::size_t kBandCount = 3;
    static constexpr double kBassFrequency = 200.0;
    static constexpr double kMidFrequency = 1000.0;
    static constexpr double kMidQ = 0.7;
    static constexpr double kTrebleFrequency = 8000.0;
    static constexpr float kMaxStepDecibels = 0.5f;

    explicit ThreeBandEq(double sampleRate = 48000.0);

    void setGainDecibels(Band band, float decibels);
    void snapToTarget();
    float preampDecibels() const;

    void process(std::span<float> samples, std::size_t channels);

private:
    void updateFilters();

    double sampleRate_;
    std::array<float, kBandCount> target_{};
    std::array<float, kBandCount> current_{};
    std::array<Biquad, kBandCount> filters_;
    Gain preamp_;
};

}
