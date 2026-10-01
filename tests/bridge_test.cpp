#include <doctest.h>

#include <optional>
#include <utility>

#include "control_bridge.h"

namespace {

struct Recorder {
    std::optional<float> gain;
    std::optional<float> balance;
    std::optional<std::pair<int, float>> eq;

    canary::ControlHandlers handlers()
    {
        return {
            .onGainChanged = [this](float v) { gain = v; },
            .onBalanceChanged = [this](float v) { balance = v; },
            .onEqChanged = [this](int band, float v) { eq = std::pair{band, v}; },
        };
    }
};

}

TEST_CASE("parseNumbers reads the JSON arrays the window sends")
{
    CHECK(canary::parseNumbers("[-8]") == std::vector<float>{-8.0f});
    CHECK(canary::parseNumbers("[1,-3.5]") == std::vector<float>{1.0f, -3.5f});
    CHECK(canary::parseNumbers("[2, 0.5]") == std::vector<float>{2.0f, 0.5f});
    CHECK(canary::parseNumbers("[]").empty());
}

TEST_CASE("parseNumbers rejects garbage")
{
    CHECK(canary::parseNumbers("[\"abc\"]").empty());
    CHECK(canary::parseNumbers("[null]").empty());
}

TEST_CASE("each control name reaches its handler with the right value")
{
    Recorder recorder;
    canary::ControlHandlers handlers = recorder.handlers();

    CHECK(canary::dispatchControl(handlers, "setGain", "[-12]"));
    CHECK(recorder.gain == -12.0f);

    CHECK(canary::dispatchControl(handlers, "setBalance", "[6.5]"));
    CHECK(recorder.balance == 6.5f);

    CHECK(canary::dispatchControl(handlers, "setEq", "[2,-4]"));
    REQUIRE(recorder.eq.has_value());
    CHECK(recorder.eq->first == 2);
    CHECK(recorder.eq->second == -4.0f);
}

TEST_CASE("invalid control calls are refused and reach no handler")
{
    Recorder recorder;
    canary::ControlHandlers handlers = recorder.handlers();

    CHECK_FALSE(canary::dispatchControl(handlers, "setVolume", "[1]"));
    CHECK_FALSE(canary::dispatchControl(handlers, "setGain", "[]"));
    CHECK_FALSE(canary::dispatchControl(handlers, "setGain", "[1,2]"));
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[3,1]"));
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[-1,1]"));
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[0.5,1]"));

    CHECK_FALSE(recorder.gain.has_value());
    CHECK_FALSE(recorder.eq.has_value());
}
