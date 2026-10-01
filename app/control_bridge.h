#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canary {

struct PlaybackStatus {
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    bool paused = false;
};

struct ControlHandlers {
    std::function<void(float)> onGainChanged;
    std::function<void(float)> onBalanceChanged;
    std::function<void(int, float)> onEqChanged;
    std::function<void(bool)> onPausedChanged;
    std::function<void(double)> onSeek;
    std::function<PlaybackStatus()> playbackStatus;
};

inline constexpr std::string_view kControlNames[] = {
    "setGain", "setBalance", "setEq", "setPaused", "seek", "getPlayback",
};

std::vector<float> parseNumbers(const std::string& request);
std::string playbackStatusJson(const PlaybackStatus& status);
std::optional<std::string> dispatchControl(const ControlHandlers& handlers, std::string_view name,
                                           const std::string& request);

}
