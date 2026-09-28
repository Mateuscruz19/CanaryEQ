#include <doctest.h>

#include <array>

#include "canary/gain.h"

TEST_CASE("default gain leaves the signal unchanged")
{
    canary::Gain gain;
    std::array<float, 4> buffer{0.5f, -0.5f, 0.25f, -0.25f};
    gain.process(buffer, 2);

    CHECK(buffer[0] == 0.5f);
    CHECK(buffer[1] == -0.5f);
    CHECK(buffer[2] == 0.25f);
    CHECK(buffer[3] == -0.25f);
}

TEST_CASE("snapped gain scales every sample in an interleaved buffer")
{
    canary::Gain gain;
    gain.setLinear(2.0f);
    gain.snapToTarget();

    std::array<float, 6> buffer{0.1f, -0.1f, 0.25f, -0.25f, 0.6f, -0.6f};
    gain.process(buffer, 2);

    CHECK(buffer[0] == doctest::Approx(0.2f));
    CHECK(buffer[1] == doctest::Approx(-0.2f));
    CHECK(buffer[4] == doctest::Approx(1.2f));
    CHECK(buffer[5] == doctest::Approx(-1.2f));
}

TEST_CASE("decibels convert to linear gain")
{
    canary::Gain gain;

    gain.setDecibels(0.0f);
    CHECK(gain.linear() == doctest::Approx(1.0f));

    gain.setDecibels(-20.0f);
    CHECK(gain.linear() == doctest::Approx(0.1f));

    gain.setDecibels(6.0206f);
    CHECK(gain.linear() == doctest::Approx(2.0f).epsilon(0.001));
}

TEST_CASE("a gain change ramps across one block instead of jumping")
{
    canary::Gain gain;
    gain.setLinear(0.0f);

    std::array<float, 8> first{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    gain.process(first, 2);

    CHECK(first[0] == doctest::Approx(0.75f));
    CHECK(first[1] == doctest::Approx(0.75f));
    CHECK(first[2] == doctest::Approx(0.5f));
    CHECK(first[3] == doctest::Approx(0.5f));
    CHECK(first[4] == doctest::Approx(0.25f));
    CHECK(first[6] == doctest::Approx(0.0f));
    CHECK(first[7] == doctest::Approx(0.0f));

    std::array<float, 2> second{1.0f, 1.0f};
    gain.process(second, 2);

    CHECK(second[0] == 0.0f);
    CHECK(second[1] == 0.0f);
}

TEST_CASE("zero gain produces silence")
{
    canary::Gain gain;
    gain.setLinear(0.0f);
    gain.snapToTarget();

    std::array<float, 2> buffer{0.9f, -0.9f};
    gain.process(buffer, 2);

    CHECK(buffer[0] == 0.0f);
    CHECK(buffer[1] == 0.0f);
}
