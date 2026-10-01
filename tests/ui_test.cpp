#include <cstdio>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "control_window.h"

namespace {

constexpr const char* kAutomation = R"js(
window.addEventListener("load", () => {
  const slider = id => document.querySelector(`#${id} input`);
  const label = id => document.querySelector(`#${id} output`).textContent;
  const move = (id, value) => {
    slider(id).value = value;
    slider(id).dispatchEvent(new Event("input"));
  };
  const reset = id => slider(id).dispatchEvent(new MouseEvent("dblclick"));

  const initialGain = Number(slider("gain").value);
  move("gain", -12);
  move("balance", 6);
  move("bass", 3);
  move("mid", -4);
  move("treble", 5);
  reset("bass");
  window.setGain(initialGain === -8 ? 1000 : -1000);
  window.setBalance(label("balance") === "R 6.0dB" ? 1000 : -1000);
  setTimeout(() => window.closeWindow(), 500);
});
)js";

int failures = 0;

void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", what.c_str());
    if (!condition) {
        ++failures;
    }
}

}

int main()
{
    std::vector<float> gains;
    std::vector<float> balances;
    std::vector<std::pair<int, float>> eqs;

    try {
        canary::runControlWindow({
            .initialGainDecibels = -8.0f,
            .handlers = {
                .onGainChanged = [&gains](float v) { gains.push_back(v); },
                .onBalanceChanged = [&balances](float v) { balances.push_back(v); },
                .onEqChanged = [&eqs](int band, float v) { eqs.emplace_back(band, v); },
            },
            .automationScript = kAutomation,
        });
    }
    catch (const std::exception& e) {
        std::printf("FAIL window could not open: %s\n", e.what());
        return 1;
    }

    check(gains.size() == 2 && gains[0] == -12.0f, "volume slider sends its value");
    check(gains.size() == 2 && gains[1] == 1000.0f, "volume slider starts at the initial gain");
    check(balances.size() == 2 && balances[0] == 6.0f, "balance slider sends its value");
    check(balances.size() == 2 && balances[1] == 1000.0f, "balance label shows the side and amount");

    std::vector<std::pair<int, float>> expectedEq{{0, 3.0f}, {1, -4.0f}, {2, 5.0f}, {0, 0.0f}};
    check(eqs == expectedEq, "bass, mid and treble sliders send their band and value, double-click resets");

    std::printf("%s\n", failures == 0 ? "all UI checks passed" : "UI checks failed");
    return failures == 0 ? 0 : 1;
}
