#include "control_window.h"

#include <webview/webview.h>

#include <cstdlib>
#include <format>
#include <string>
#include <utility>

namespace canary {

namespace {

constexpr const char* kHtml = R"html(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Manrope:wght@500;700;800&display=swap" rel="stylesheet">
<style>
  :root {
    color-scheme: dark;
    --bg: #0d0e11;
    --track: #24272e;
    --text: #ecedef;
    --muted: #858a94;
    --accent: #f2c14e;
  }
  * { box-sizing: border-box; margin: 0; }
  html, body { height: 100%; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: Manrope, "Segoe UI", sans-serif;
    display: flex;
    align-items: center;
    user-select: none;
  }
  main { width: 100%; padding: 0 32px; display: grid; gap: 30px; }
  .group { display: grid; gap: 22px; padding-top: 26px; border-top: 1px solid var(--track); }
  header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 18px; }
  h1 { font-size: 12px; font-weight: 700; letter-spacing: .14em; text-transform: uppercase; color: var(--muted); }
  output { font-size: 30px; font-weight: 800; font-variant-numeric: tabular-nums; letter-spacing: -.02em; }
  output small { font-size: 14px; font-weight: 500; color: var(--muted); margin-left: 6px; }
  input[type=range] {
    --from: 0%;
    --to: 0%;
    width: 100%;
    height: 6px;
    appearance: none;
    border-radius: 3px;
    outline: none;
    background: linear-gradient(to right,
      var(--track) var(--from), var(--accent) var(--from),
      var(--accent) var(--to), var(--track) var(--to));
  }
  input[type=range]::-webkit-slider-thumb {
    appearance: none;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: var(--text);
    border: 4px solid var(--accent);
    cursor: grab;
  }
  input[type=range]:active::-webkit-slider-thumb { cursor: grabbing; }
  .scale { position: relative; height: 16px; margin-top: 12px; font-size: 11px; color: var(--muted); font-variant-numeric: tabular-nums; }
  .scale span { position: absolute; transform: translateX(-50%); }
  footer { font-size: 11px; color: var(--muted); }
</style>
</head>
<body>
<main>
  <section id="gain"></section>
  <section id="balance"></section>
  <div class="group">
    <section id="bass"></section>
    <section id="mid"></section>
    <section id="treble"></section>
  </div>
  <footer>Double-click a slider to reset it</footer>
</main>
<script>
  const signed = v => `${v > 0 ? "+" : ""}${v.toFixed(1)}`;

  const makeSlider = ({ id, title, min, max, step, initial, origin, ticks, label, onChange }) => {
    const section = document.getElementById(id);
    section.innerHTML = `
      <header><h1>${title}</h1><output></output></header>
      <input type="range" min="${min}" max="${max}" step="${step}">
      <div class="scale"></div>`;
    const output = section.querySelector("output");
    const slider = section.querySelector("input");
    const scale = section.querySelector(".scale");
    const percent = v => (v - min) / (max - min) * 100;

    for (const [value, text] of ticks) {
      const tick = document.createElement("span");
      tick.textContent = text;
      tick.style.left = `${percent(value)}%`;
      scale.appendChild(tick);
    }

    const show = v => {
      output.innerHTML = label(v);
      slider.style.setProperty("--from", `${Math.min(percent(origin), percent(v))}%`);
      slider.style.setProperty("--to", `${Math.max(percent(origin), percent(v))}%`);
    };

    const apply = v => {
      show(v);
      onChange(v);
    };

    slider.value = initial;
    show(initial);
    slider.addEventListener("input", () => apply(Number(slider.value)));
    slider.addEventListener("dblclick", () => {
      slider.value = origin;
      apply(origin);
    });
  };

  makeSlider({
    id: "gain",
    title: "Volume",
    min: -40, max: 6, step: 0.5,
    initial: window.initialGain,
    origin: -40,
    ticks: [[-40, "-40"], [-30, "-30"], [-20, "-20"], [-10, "-10"], [0, "0"], [6, "+6"]],
    label: v => `${signed(v)}<small>dB</small>`,
    onChange: v => window.setGain(v),
  });

  makeSlider({
    id: "balance",
    title: "Balance",
    min: -12, max: 12, step: 0.5,
    initial: 0,
    origin: 0,
    ticks: [[-12, "L 12"], [-6, "6"], [0, "C"], [6, "6"], [12, "12 R"]],
    label: v => v === 0 ? "Center" : `${v < 0 ? "L" : "R"} ${Math.abs(v).toFixed(1)}<small>dB</small>`,
    onChange: v => window.setBalance(v),
  });

  [["bass", "Bass"], ["mid", "Mid"], ["treble", "Treble"]].forEach(([id, title], band) => makeSlider({
    id,
    title,
    min: -12, max: 12, step: 0.5,
    initial: 0,
    origin: 0,
    ticks: [[-12, "-12"], [-6, "-6"], [0, "0"], [6, "+6"], [12, "+12"]],
    label: v => `${signed(v)}<small>dB</small>`,
    onChange: v => window.setEq(band, v),
  }));
</script>
</body>
</html>
)html";

float parseFirstNumber(const std::string& request)
{
    return std::strtof(request.c_str() + 1, nullptr);
}

std::pair<int, float> parseIndexAndNumber(const std::string& request)
{
    char* end = nullptr;
    long index = std::strtol(request.c_str() + 1, &end, 10);
    float value = std::strtof(end + 1, nullptr);
    return {static_cast<int>(index), value};
}

}

void runControlWindow(const ControlWindowOptions& options)
{
    webview::webview window(false, nullptr);
    window.set_title("CanaryEQ");
    window.set_size(440, 640, WEBVIEW_HINT_FIXED);
    window.bind("setGain", [&options](const std::string& request) -> std::string {
        options.onGainChanged(parseFirstNumber(request));
        return "null";
    });
    window.bind("setBalance", [&options](const std::string& request) -> std::string {
        options.onBalanceChanged(parseFirstNumber(request));
        return "null";
    });
    window.bind("setEq", [&options](const std::string& request) -> std::string {
        auto [band, decibels] = parseIndexAndNumber(request);
        options.onEqChanged(band, decibels);
        return "null";
    });
    window.init(std::format("window.initialGain = {};", options.initialGainDecibels));
    window.set_html(kHtml);
    window.run();
}

}
