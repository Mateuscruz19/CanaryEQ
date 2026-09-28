#include <doctest.h>

#include <array>

#include "canary/balance.h"

TEST_CASE("centered balance leaves both sides unchanged")
{
    canary::Balance balance;
    std::array<float, 4> buffer{0.5f, -0.5f, 0.25f, -0.25f};
    balance.process(buffer, 2);

    CHECK(buffer[0] == 0.5f);
    CHECK(buffer[1] == -0.5f);
    CHECK(buffer[2] == 0.25f);
    CHECK(buffer[3] == -0.25f);
}

TEST_CASE("shifting right only cuts the left side")
{
    canary::Balance balance;
    balance.setDecibels(6.0206f);

    CHECK(balance.leftLinear() == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(balance.rightLinear() == 1.0f);
}

TEST_CASE("shifting left only cuts the right side")
{
    canary::Balance balance;
    balance.setDecibels(-20.0f);

    CHECK(balance.leftLinear() == 1.0f);
    CHECK(balance.rightLinear() == doctest::Approx(0.1f));
}

TEST_CASE("snapped balance scales each side of an interleaved buffer")
{
    canary::Balance balance;
    balance.setDecibels(-20.0f);
    balance.snapToTarget();

    std::array<float, 4> buffer{0.8f, 0.8f, -0.4f, -0.4f};
    balance.process(buffer, 2);

    CHECK(buffer[0] == doctest::Approx(0.8f));
    CHECK(buffer[1] == doctest::Approx(0.08f));
    CHECK(buffer[2] == doctest::Approx(-0.4f));
    CHECK(buffer[3] == doctest::Approx(-0.04f));
}

TEST_CASE("a balance change ramps across one block")
{
    canary::Balance balance;
    balance.setDecibels(-120.0f);

    std::array<float, 4> buffer{1.0f, 1.0f, 1.0f, 1.0f};
    balance.process(buffer, 2);

    CHECK(buffer[0] == 1.0f);
    CHECK(buffer[1] == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(buffer[2] == 1.0f);
    CHECK(buffer[3] == doctest::Approx(0.0f));
}

TEST_CASE("balance ignores anything that is not stereo")
{
    canary::Balance balance;
    balance.setDecibels(-20.0f);
    balance.snapToTarget();

    std::array<float, 3> mono{0.3f, 0.6f, 0.9f};
    balance.process(mono, 1);

    CHECK(mono[0] == 0.3f);
    CHECK(mono[1] == 0.6f);
    CHECK(mono[2] == 0.9f);
}
