import { prepare } from "./runtime.js";

const frame = document.querySelector("#runtime-frame");
const canvas = document.querySelector("#game-canvas");
const loading = document.querySelector("#loading");
const play = document.querySelector("#play");
const status = document.querySelector("#status");
const fullscreen = document.querySelector("#fullscreen");
let runtime = null;
let startRuntime = null;

function report(message) {
  status.textContent = message;
}

function reportResolution(resolution) {
  report(`RUNNING / ${resolution.width}×${resolution.height}`);
}

async function load() {
  report("LOADING GAME…");
  startRuntime = await prepare({
    canvas,
    onStopped: () => {
      runtime = null;
      frame.dataset.state = "error";
      loading.querySelector("strong").textContent = "GAME CLOSED";
      loading.querySelector("span").textContent = "reload this page to begin again";
      report("GAME CLOSED");
    },
    onResize: reportResolution,
  });
  frame.dataset.state = "ready";
  report("READY");
}

fullscreen.addEventListener("click", async () => {
  if (document.fullscreenElement)
    await document.exitFullscreen();
  else
    await frame.requestFullscreen();
  canvas.focus();
});

document.addEventListener("fullscreenchange", () => {
  fullscreen.textContent = document.fullscreenElement ? "EXIT FULLSCREEN" : "FULLSCREEN";
});

play.addEventListener("click", () => {
  if (runtime || !startRuntime)
    return;
  runtime = startRuntime();
  runtime.resumeAudio();
  frame.dataset.state = "running";
  reportResolution(runtime.resolution());
  canvas.focus();
});

load().catch((error) => {
  console.error(error);
  frame.dataset.state = "error";
  loading.querySelector("strong").textContent = "COULD NOT START";
  loading.querySelector("span").textContent = error.message;
  report("STARTUP ERROR");
});
