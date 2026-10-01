#pragma once

#include <functional>

namespace canary {

struct ControlWindowOptions {
    float initialGainDecibels = 0.0f;
    std::function<void(float)> onGainChanged;
    std::function<void(float)> onBalanceChanged;
    std::function<void(int, float)> onEqChanged;
};

void runControlWindow(const ControlWindowOptions& options);

}
