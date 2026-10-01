#include <doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "canary/biquad.h"
#include "canary/three_band_eq.h"

namespace {

constexpr double kSampleRate = 48000.0;

std::vector<float> sine(double frequency, std::size_t length)
{
    std::vector<float> samples(length);
    for (std::size_t n = 0; n < length; ++n) {
        samples[n] = static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * frequency * n / kSampleRate));
    }
    return samples;
}

double peak(const std::vector<float>& samples, std::size_t from)
{
    double result = 0.0;
    for (std::size_t n = from; n < samples.size(); ++n) {
        result = std::max(result, static_cast<double>(std::fabs(samples[n])));
    }
    return result;
}

double gainDecibelsAt(canary::Biquad& filter, double frequency)
{
    std::vector<float> samples = sine(frequency, 96000);
    filter.process(samples, 1);
    return 20.0 * std::log10(peak(samples, 48000) / 0.25);
}

}

TEST_CASE("a flat biquad passes the signal through")
{
    for (const canary::BiquadCoefficients& coefficients :
         {canary::lowShelf(kSampleRate, 100.0, 0.0), canary::highShelf(kSampleRate, 8000.0, 0.0),
          canary::peaking(kSampleRate, 1000.0, 0.0, 0.7)}) {
        CHECK(coefficients.b0 == doctest::Approx(1.0));
        CHECK(coefficients.b1 == doctest::Approx(coefficients.a1));
        CHECK(coefficients.b2 == doctest::Approx(coefficients.a2));
    }
}

TEST_CASE("a low shelf boosts the bass and leaves the treble alone")
{
    canary::Biquad bass;
    bass.setCoefficients(canary::lowShelf(kSampleRate, 100.0, 12.0));
    CHECK(gainDecibelsAt(bass, 20.0) == doctest::Approx(12.0).epsilon(0.05));

    canary::Biquad treble;
    treble.setCoefficients(canary::lowShelf(kSampleRate, 100.0, 12.0));
    CHECK(gainDecibelsAt(treble, 5000.0) == doctest::Approx(0.0).epsilon(0.05));
}

TEST_CASE("a high shelf cuts the treble and leaves the bass alone")
{
    canary::Biquad treble;
    treble.setCoefficients(canary::highShelf(kSampleRate, 8000.0, -12.0));
    CHECK(gainDecibelsAt(treble, 18000.0) == doctest::Approx(-12.0).epsilon(0.05));

    canary::Biquad bass;
    bass.setCoefficients(canary::highShelf(kSampleRate, 8000.0, -12.0));
    CHECK(gainDecibelsAt(bass, 100.0) == doctest::Approx(0.0).epsilon(0.05));
}

TEST_CASE("a peaking filter reaches its gain at its center frequency only")
{
    canary::Biquad center;
    center.setCoefficients(canary::peaking(kSampleRate, 1000.0, 6.0, 0.7));
    CHECK(gainDecibelsAt(center, 1000.0) == doctest::Approx(6.0).epsilon(0.05));

    canary::Biquad far;
    far.setCoefficients(canary::peaking(kSampleRate, 1000.0, 6.0, 0.7));
    CHECK(gainDecibelsAt(far, 30.0) < 0.5);
}

TEST_CASE("the equalizer lowers the preamp by its loudest boost")
{
    canary::ThreeBandEq eq;
    eq.setGainDecibels(canary::Band::Bass, 6.0f);
    eq.setGainDecibels(canary::Band::Treble, 3.0f);
    eq.snapToTarget();
    CHECK(eq.preampDecibels() == doctest::Approx(-6.0f));

    eq.setGainDecibels(canary::Band::Bass, -9.0f);
    eq.setGainDecibels(canary::Band::Treble, -3.0f);
    eq.snapToTarget();
    CHECK(eq.preampDecibels() == 0.0f);
}

TEST_CASE("a flat equalizer leaves the signal unchanged")
{
    canary::ThreeBandEq eq;
    std::vector<float> samples = sine(440.0, 4800);
    std::vector<float> original = samples;
    eq.process(samples, 1);

    for (std::size_t n = 0; n < samples.size(); ++n) {
        CHECK(samples[n] == doctest::Approx(original[n]).epsilon(1e-5));
    }
}
