#include "control_bridge.h"

#include <cmath>
#include <cstdlib>
#include <format>

#include "canary/three_band_eq.h"

namespace canary {

namespace {

constexpr const char* kNull = "null";

bool isWhole(float value)
{
    return value == std::floor(value);
}

}

std::vector<float> parseNumbers(const std::string& request)
{
    std::vector<float> numbers;
    const char* cursor = request.c_str();
    while (*cursor != '\0') {
        if (*cursor == '[' || *cursor == ',' || *cursor == ' ' || *cursor == ']') {
            ++cursor;
            continue;
        }
        if (std::string_view(cursor).starts_with("true")) {
            numbers.push_back(1.0f);
            cursor += 4;
            continue;
        }
        if (std::string_view(cursor).starts_with("false")) {
            numbers.push_back(0.0f);
            cursor += 5;
            continue;
        }
        char* end = nullptr;
        float value = std::strtof(cursor, &end);
        if (end == cursor || !std::isfinite(value)) {
            return {};
        }
        numbers.push_back(value);
        cursor = end;
    }
    return numbers;
}

std::string playbackStatusJson(const PlaybackStatus& status)
{
    return std::format(R"({{"position":{:.3f},"duration":{:.3f},"paused":{}}})", status.positionSeconds,
                       status.durationSeconds, status.paused);
}

std::optional<std::string> dispatchControl(const ControlHandlers& handlers, std::string_view name,
                                           const std::string& request)
{
    std::vector<float> numbers = parseNumbers(request);

    if (name == "setGain" && numbers.size() == 1 && handlers.onGainChanged) {
        handlers.onGainChanged(numbers[0]);
        return kNull;
    }
    if (name == "setBalance" && numbers.size() == 1 && handlers.onBalanceChanged) {
        handlers.onBalanceChanged(numbers[0]);
        return kNull;
    }
    if (name == "setEq" && numbers.size() == 2 && handlers.onEqChanged) {
        float band = numbers[0];
        if (!isWhole(band) || band < 0.0f || band >= static_cast<float>(ThreeBandEq::kBandCount)) {
            return std::nullopt;
        }
        handlers.onEqChanged(static_cast<int>(band), numbers[1]);
        return kNull;
    }
    if (name == "setPaused" && numbers.size() == 1 && handlers.onPausedChanged) {
        handlers.onPausedChanged(numbers[0] != 0.0f);
        return kNull;
    }
    if (name == "seek" && numbers.size() == 1 && numbers[0] >= 0.0f && handlers.onSeek) {
        handlers.onSeek(numbers[0]);
        return kNull;
    }
    if (name == "getPlayback" && numbers.empty() && handlers.playbackStatus) {
        return playbackStatusJson(handlers.playbackStatus());
    }
    return std::nullopt;
}

}
