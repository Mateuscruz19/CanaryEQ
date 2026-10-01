#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace canary {

struct ControlHandlers {
    std::function<void(float)> onGainChanged;
    std::function<void(float)> onBalanceChanged;
    std::function<void(int, float)> onEqChanged;
};

inline constexpr std::string_view kControlNames[] = {"setGain", "setBalance", "setEq"};

std::vector<float> parseNumbers(const std::string& request);
bool dispatchControl(const ControlHandlers& handlers, std::string_view name, const std::string& request);

}
