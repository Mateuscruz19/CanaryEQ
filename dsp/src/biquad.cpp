#include "canary/biquad.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace canary {

namespace {

struct ShelfTerms {
    double a;
    double cosine;
    double twoSqrtAAlpha;
};

ShelfTerms shelfTerms(double sampleRate, double frequency, double gainDecibels)
{
    double a = std::pow(10.0, gainDecibels / 40.0);
    double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    double alpha = std::sin(omega) / 2.0 * std::numbers::sqrt2;
    return {a, std::cos(omega), 2.0 * std::sqrt(a) * alpha};
}

BiquadCoefficients normalize(double b0, double b1, double b2, double a0, double a1, double a2)
{
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

}

BiquadCoefficients lowShelf(double sampleRate, double frequency, double gainDecibels)
{
    auto [a, c, k] = shelfTerms(sampleRate, frequency, gainDecibels);
    return normalize(a * ((a + 1) - (a - 1) * c + k),
                     2 * a * ((a - 1) - (a + 1) * c),
                     a * ((a + 1) - (a - 1) * c - k),
                     (a + 1) + (a - 1) * c + k,
                     -2 * ((a - 1) + (a + 1) * c),
                     (a + 1) + (a - 1) * c - k);
}

BiquadCoefficients highShelf(double sampleRate, double frequency, double gainDecibels)
{
    auto [a, c, k] = shelfTerms(sampleRate, frequency, gainDecibels);
    return normalize(a * ((a + 1) + (a - 1) * c + k),
                     -2 * a * ((a - 1) + (a + 1) * c),
                     a * ((a + 1) + (a - 1) * c - k),
                     (a + 1) - (a - 1) * c + k,
                     2 * ((a - 1) - (a + 1) * c),
                     (a + 1) - (a - 1) * c - k);
}

BiquadCoefficients peaking(double sampleRate, double frequency, double gainDecibels, double q)
{
    double a = std::pow(10.0, gainDecibels / 40.0);
    double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    double alpha = std::sin(omega) / (2.0 * q);
    double c = std::cos(omega);
    return normalize(1 + alpha * a, -2 * c, 1 - alpha * a, 1 + alpha / a, -2 * c, 1 - alpha / a);
}

void Biquad::setCoefficients(const BiquadCoefficients& coefficients)
{
    coefficients_ = coefficients;
}

void Biquad::reset()
{
    z1_.fill(0.0);
    z2_.fill(0.0);
}

void Biquad::process(std::span<float> samples, std::size_t channels)
{
    if (channels == 0) {
        return;
    }

    const auto [b0, b1, b2, a1, a2] = coefficients_;
    std::size_t filtered = std::min(channels, kMaxChannels);
    std::size_t frames = samples.size() / channels;

    for (std::size_t channel = 0; channel < filtered; ++channel) {
        double z1 = z1_[channel];
        double z2 = z2_[channel];
        for (std::size_t frame = 0; frame < frames; ++frame) {
            float& sample = samples[frame * channels + channel];
            double x = sample;
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            sample = static_cast<float>(y);
        }
        z1_[channel] = z1;
        z2_[channel] = z2;
    }
}

}
