#include "canary/three_band_eq.h"

#include <algorithm>

namespace canary {

ThreeBandEq::ThreeBandEq(double sampleRate)
    : sampleRate_(sampleRate)
{
    updateFilters();
}

void ThreeBandEq::setGainDecibels(Band band, float decibels)
{
    target_[static_cast<std::size_t>(band)] = decibels;
}

void ThreeBandEq::snapToTarget()
{
    current_ = target_;
    updateFilters();
    preamp_.snapToTarget();
}

float ThreeBandEq::preampDecibels() const
{
    float loudestBoost = *std::max_element(current_.begin(), current_.end());
    return -std::max(0.0f, loudestBoost);
}

void ThreeBandEq::process(std::span<float> samples, std::size_t channels)
{
    bool changed = false;
    for (std::size_t band = 0; band < kBandCount; ++band) {
        float difference = target_[band] - current_[band];
        if (difference != 0.0f) {
            current_[band] += std::clamp(difference, -kMaxStepDecibels, kMaxStepDecibels);
            changed = true;
        }
    }
    if (changed) {
        updateFilters();
    }

    preamp_.process(samples, channels);
    for (Biquad& filter : filters_) {
        filter.process(samples, channels);
    }
}

void ThreeBandEq::updateFilters()
{
    filters_[static_cast<std::size_t>(Band::Bass)].setCoefficients(
        lowShelf(sampleRate_, kBassFrequency, current_[static_cast<std::size_t>(Band::Bass)]));
    filters_[static_cast<std::size_t>(Band::Mid)].setCoefficients(
        peaking(sampleRate_, kMidFrequency, current_[static_cast<std::size_t>(Band::Mid)], kMidQ));
    filters_[static_cast<std::size_t>(Band::Treble)].setCoefficients(
        highShelf(sampleRate_, kTrebleFrequency, current_[static_cast<std::size_t>(Band::Treble)]));
    preamp_.setDecibels(preampDecibels());
}

}
