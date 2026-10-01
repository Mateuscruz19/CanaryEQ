#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace canary {

struct BiquadCoefficients {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

BiquadCoefficients lowShelf(double sampleRate, double frequency, double gainDecibels);
BiquadCoefficients highShelf(double sampleRate, double frequency, double gainDecibels);
BiquadCoefficients peaking(double sampleRate, double frequency, double gainDecibels, double q);

class Biquad {
public:
    static constexpr std::size_t kMaxChannels = 8;

    void setCoefficients(const BiquadCoefficients& coefficients);
    void reset();
    void process(std::span<float> samples, std::size_t channels);

private:
    BiquadCoefficients coefficients_;
    std::array<double, kMaxChannels> z1_{};
    std::array<double, kMaxChannels> z2_{};
};

}
