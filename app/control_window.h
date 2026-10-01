#pragma once

#include <functional>
#include <string>

#include "control_bridge.h"

namespace canary {

struct ControlWindowOptions {
    float initialGainDecibels = 0.0f;
    std::string trackName;
    ControlHandlers handlers;
    std::string automationScript;
    std::function<void(const std::string&)> onAutomationReport;
};

void runControlWindow(const ControlWindowOptions& options);

}
