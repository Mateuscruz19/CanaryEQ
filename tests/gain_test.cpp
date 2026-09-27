#include <doctest.h>

#include <array>

#include "canary/gain.h"

TEST_CASE("default gain is unity")
{
    canary::Gain gain;
    CHECK(gain.processSample(0.5f) == 0.5f);
}

TEST_CASE("gain scales a single sample")
{
    canary::Gain gain;
    gain.setLinear(0.5f);
    CHECK(gain.processSample(0.8f) == doctest::Approx(0.4f));
    CHECK(gain.processSample(-0.8f) == doctest::Approx(-0.4f));
}

TEST_CASE("gain scales every sample in an interleaved buffer")
{
    canary::Gain gain;
    gain.setLinear(2.0f);

    std::array<float, 6> buffer{0.1f, -0.1f, 0.25f, -0.25f, 0.6f, -0.6f};
    gain.process(buffer);

    CHECK(buffer[0] == doctest::Approx(0.2f));
    CHECK(buffer[1] == doctest::Approx(-0.2f));
    CHECK(buffer[4] == doctest::Approx(1.2f));
    CHECK(buffer[5] == doctest::Approx(-1.2f));
}

TEST_CASE("zero gain produces silence")
{
    canary::Gain gain;
    gain.setLinear(0.0f);
    CHECK(gain.processSample(0.9f) == 0.0f);
}
