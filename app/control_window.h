#pragma once

#include <string>

#include "control_bridge.h"

namespace canary {

struct ControlWindowOptions {
    float initialGainDecibels = 0.0f;
    ControlHandlers handlers;
    std::string automationScript;
};

void runControlWindow(const ControlWindowOptions& options);

}
