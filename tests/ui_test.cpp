#include <cstdio>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "control_window.h"

namespace {

constexpr const char* kAutomation = R"js(
window.addEventListener("load", async () => {
  const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
  const report = (key, value) => window.reportResult(key, String(value));
  const slider = id => document.querySelector(`#${id} input`);
  const label = id => document.querySelector(`#${id} output`).textContent;
  const move = (id, value) => {
    slider(id).value = value;
    slider(id).dispatchEvent(new Event("input"));
  };
  const reset = id => slider(id).dispatchEvent(new MouseEvent("dblclick"));

  report("initialGain", slider("gain").value);
  move("gain", -12);
  move("balance", 6);
  report("balanceLabel", label("balance"));
  move("bass", 3);
  move("mid", -4);
  move("treble", 5);
  reset("bass");

  await wait(500);
  report("track", document.getElementById("track").textContent);
  report("time", document.getElementById("time").textContent);

  document.getElementById("play").click();
  await wait(400);
  report("playLabel", document.getElementById("play").getAttribute("aria-label"));
  document.dispatchEvent(new KeyboardEvent("keydown", { code: "Space" }));

  const seek = document.getElementById("seek");
  seek.value = 30;
  seek.dispatchEvent(new Event("change"));

  await wait(300);
  window.closeWindow();
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

bool reported(const std::vector<std::string>& reports, const std::string& key, const std::string& value)
{
    std::string expected = "[\"" + key + "\",\"" + value + "\"]";
    for (const std::string& report : reports) {
        if (report == expected) {
            return true;
        }
    }
    return false;
}

}

int main()
{
    std::vector<float> gains;
    std::vector<float> balances;
    std::vector<std::pair<int, float>> eqs;
    std::vector<bool> pauses;
    std::vector<double> seeks;
    std::vector<std::string> reports;
    bool paused = false;

    try {
        canary::runControlWindow({
            .initialGainDecibels = -8.0f,
            .trackName = "Test Track",
            .handlers = {
                .onGainChanged = [&](float v) { gains.push_back(v); },
                .onBalanceChanged = [&](float v) { balances.push_back(v); },
                .onEqChanged = [&](int band, float v) { eqs.emplace_back(band, v); },
                .onPausedChanged =
                    [&](bool v) {
                        pauses.push_back(v);
                        paused = v;
                    },
                .onSeek = [&](double v) { seeks.push_back(v); },
                .playbackStatus =
                    [&] {
                        return canary::PlaybackStatus{.positionSeconds = 75.25, .durationSeconds = 198.0, .paused = paused};
                    },
            },
            .automationScript = kAutomation,
            .onAutomationReport = [&](const std::string& report) { reports.push_back(report); },
        });
    }
    catch (const std::exception& e) {
        std::printf("FAIL window could not open: %s\n", e.what());
        return 1;
    }

    check(reported(reports, "initialGain", "-8"), "volume slider starts at the initial gain");
    check(gains == std::vector<float>{-12.0f}, "volume slider sends its value");
    check(balances == std::vector<float>{6.0f}, "balance slider sends its value");
    check(reported(reports, "balanceLabel", "R 6.0dB"), "balance label shows the side and amount");

    std::vector<std::pair<int, float>> expectedEq{{0, 3.0f}, {1, -4.0f}, {2, 5.0f}, {0, 0.0f}};
    check(eqs == expectedEq, "bass, mid and treble sliders send their band and value, double-click resets");

    check(reported(reports, "track", "Test Track"), "the track name is shown");
    check(reported(reports, "time", "1:15 / 3:18"), "the time follows the player position");
    check(reported(reports, "playLabel", "Play"), "the play button switches to Play after pausing");
    check(pauses == std::vector<bool>{true, false}, "the play button pauses and the space bar resumes");
    check(seeks == std::vector<double>{30.0}, "releasing the position slider seeks");

    std::printf("%s\n", failures == 0 ? "all UI checks passed" : "UI checks failed");
    return failures == 0 ? 0 : 1;
}
