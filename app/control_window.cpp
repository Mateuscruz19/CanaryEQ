#include "control_window.h"

#include <webview/webview.h>

#include <cstdlib>
#include <format>
#include <string>

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
    --fill: 0%;
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
  main { width: 100%; padding: 0 32px; }
  header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 26px; }
  h1 { font-size: 12px; font-weight: 700; letter-spacing: .14em; text-transform: uppercase; color: var(--muted); }
  output { font-size: 38px; font-weight: 800; font-variant-numeric: tabular-nums; letter-spacing: -.02em; }
  output small { font-size: 15px; font-weight: 500; color: var(--muted); margin-left: 6px; }
  input[type=range] {
    width: 100%;
    height: 6px;
    appearance: none;
    border-radius: 3px;
    outline: none;
    background: linear-gradient(to right, var(--accent) var(--fill), var(--track) var(--fill));
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
  footer { margin-top: 22px; font-size: 11px; color: var(--muted); }
</style>
</head>
<body>
<main>
  <header>
    <h1>Volume</h1>
    <output id="value"></output>
  </header>
  <input id="gain" type="range" min="-40" max="6" step="0.5">
  <div class="scale" id="scale"></div>
  <footer>Double-click the slider to reset to 0 dB</footer>
</main>
<script>
  const slider = document.getElementById("gain");
  const value = document.getElementById("value");
  const scale = document.getElementById("scale");
  const min = Number(slider.min);
  const max = Number(slider.max);
  const percent = v => (v - min) / (max - min) * 100;

  for (const tick of [-40, -30, -20, -10, 0, 6]) {
    const label = document.createElement("span");
    label.textContent = tick > 0 ? `+${tick}` : `${tick}`;
    label.style.left = `${percent(tick)}%`;
    scale.appendChild(label);
  }

  const show = v => {
    value.innerHTML = `${v > 0 ? "+" : ""}${v.toFixed(1)}<small>dB</small>`;
    slider.style.setProperty("--fill", `${percent(v)}%`);
  };

  const apply = v => {
    show(v);
    window.setGain(v);
  };

  slider.value = window.initialGain;
  show(Number(slider.value));
  slider.addEventListener("input", () => apply(Number(slider.value)));
  slider.addEventListener("dblclick", () => {
    slider.value = 0;
    apply(0);
  });
</script>
</body>
</html>
)html";

}

void runControlWindow(float initialDecibels, const std::function<void(float)>& onGainChanged)
{
    webview::webview window(false, nullptr);
    window.set_title("CanaryEQ");
    window.set_size(440, 230, WEBVIEW_HINT_FIXED);
    window.bind("setGain", [&onGainChanged](const std::string& request) -> std::string {
        onGainChanged(std::strtof(request.c_str() + 1, nullptr));
        return "null";
    });
    window.init(std::format("window.initialGain = {};", initialDecibels));
    window.set_html(kHtml);
    window.run();
}

}
