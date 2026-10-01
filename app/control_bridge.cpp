#include "control_bridge.h"

#include <cmath>
#include <cstdlib>

#include "canary/three_band_eq.h"

namespace canary {

std::vector<float> parseNumbers(const std::string& request)
{
    std::vector<float> numbers;
    const char* cursor = request.c_str();
    while (*cursor != '\0') {
        if (*cursor == '[' || *cursor == ',' || *cursor == ' ' || *cursor == ']') {
            ++cursor;
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

bool dispatchControl(const ControlHandlers& handlers, std::string_view name, const std::string& request)
{
    std::vector<float> numbers = parseNumbers(request);

    if (name == "setGain" && numbers.size() == 1 && handlers.onGainChanged) {
        handlers.onGainChanged(numbers[0]);
        return true;
    }
    if (name == "setBalance" && numbers.size() == 1 && handlers.onBalanceChanged) {
        handlers.onBalanceChanged(numbers[0]);
        return true;
    }
    if (name == "setEq" && numbers.size() == 2 && handlers.onEqChanged) {
        float band = numbers[0];
        if (band != std::floor(band) || band < 0.0f || band >= static_cast<float>(ThreeBandEq::kBandCount)) {
            return false;
        }
        handlers.onEqChanged(static_cast<int>(band), numbers[1]);
        return true;
    }
    return false;
}

}
