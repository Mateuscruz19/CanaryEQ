#include <doctest.h>

#include <optional>
#include <string>
#include <utility>

#include "control_bridge.h"

namespace {

struct Recorder {
    std::optional<float> gain;
    std::optional<float> balance;
    std::optional<std::pair<int, float>> eq;
    std::optional<bool> paused;
    std::optional<double> seek;
    canary::PlaybackStatus status{.positionSeconds = 75.25, .durationSeconds = 198.0, .paused = true};

    canary::ControlHandlers handlers()
    {
        return {
            .onGainChanged = [this](float v) { gain = v; },
            .onBalanceChanged = [this](float v) { balance = v; },
            .onEqChanged = [this](int band, float v) { eq = std::pair{band, v}; },
            .onPausedChanged = [this](bool v) { paused = v; },
            .onSeek = [this](double v) { seek = v; },
            .playbackStatus = [this] { return status; },
        };
    }
};

}

TEST_CASE("parseNumbers reads the JSON arrays the window sends")
{
    CHECK(canary::parseNumbers("[-8]") == std::vector<float>{-8.0f});
    CHECK(canary::parseNumbers("[1,-3.5]") == std::vector<float>{1.0f, -3.5f});
    CHECK(canary::parseNumbers("[2, 0.5]") == std::vector<float>{2.0f, 0.5f});
    CHECK(canary::parseNumbers("[true]") == std::vector<float>{1.0f});
    CHECK(canary::parseNumbers("[false]") == std::vector<float>{0.0f});
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

    CHECK(canary::dispatchControl(handlers, "setGain", "[-12]") == "null");
    CHECK(recorder.gain == -12.0f);

    CHECK(canary::dispatchControl(handlers, "setBalance", "[6.5]") == "null");
    CHECK(recorder.balance == 6.5f);

    CHECK(canary::dispatchControl(handlers, "setEq", "[2,-4]") == "null");
    REQUIRE(recorder.eq.has_value());
    CHECK(recorder.eq->first == 2);
    CHECK(recorder.eq->second == -4.0f);

    CHECK(canary::dispatchControl(handlers, "setPaused", "[true]") == "null");
    CHECK(recorder.paused == true);
    CHECK(canary::dispatchControl(handlers, "setPaused", "[false]") == "null");
    CHECK(recorder.paused == false);

    CHECK(canary::dispatchControl(handlers, "seek", "[42.5]") == "null");
    CHECK(recorder.seek == 42.5);
}

TEST_CASE("getPlayback answers with the player status as JSON")
{
    Recorder recorder;
    CHECK(canary::dispatchControl(recorder.handlers(), "getPlayback", "[]") ==
          std::string(R"({"position":75.250,"duration":198.000,"paused":true})"));
}

TEST_CASE("every control name the window binds is understood by the bridge")
{
    Recorder recorder;
    canary::ControlHandlers handlers = recorder.handlers();
    const std::pair<std::string_view, std::string> valid[] = {
        {"setGain", "[0]"},     {"setBalance", "[0]"}, {"setEq", "[0,0]"},
        {"setPaused", "[false]"}, {"seek", "[0]"},     {"getPlayback", "[]"},
    };
    for (std::string_view name : canary::kControlNames) {
        bool covered = false;
        for (const auto& [validName, request] : valid) {
            if (validName == name) {
                covered = canary::dispatchControl(handlers, name, request).has_value();
            }
        }
        CHECK_MESSAGE(covered, std::string(name));
    }
}

TEST_CASE("invalid control calls are refused and reach no handler")
{
    Recorder recorder;
    canary::ControlHandlers handlers = recorder.handlers();

    CHECK_FALSE(canary::dispatchControl(handlers, "setVolume", "[1]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "setGain", "[]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "setGain", "[1,2]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[3,1]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[-1,1]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "setEq", "[0.5,1]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "seek", "[-5]").has_value());
    CHECK_FALSE(canary::dispatchControl(handlers, "getPlayback", "[1]").has_value());

    CHECK_FALSE(recorder.gain.has_value());
    CHECK_FALSE(recorder.eq.has_value());
    CHECK_FALSE(recorder.seek.has_value());
}
