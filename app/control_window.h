#pragma once

#include <functional>

namespace canary {

void runControlWindow(float initialDecibels, const std::function<void(float)>& onGainChanged);

}
