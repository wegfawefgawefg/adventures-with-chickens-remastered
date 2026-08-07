const bundleRoot = new URL("runtime/", import.meta.url);

async function loadModuleFactory() {
  const response = await fetch(new URL("manifest.json", bundleRoot));
  if (!response.ok)
    throw new Error(`runtime manifest request returned ${response.status}`);
  const manifest = await response.json();
  if (!manifest.module || !manifest.wasm || !manifest.data)
    throw new Error("runtime manifest is incomplete");
  return import(new URL(manifest.module, bundleRoot).href);
}

function syncFilesystem(module, populate) {
  return new Promise((resolve, reject) => {
    module.FS.syncfs(populate, (error) => error ? reject(error) : resolve());
  });
}

async function mountStorage(module) {
  const path = "/home/web_user/.local/share/adventures-with-chickens-remastered";
  module.FS.mkdirTree(path);
  module.FS.mount(module.FS.filesystems.IDBFS, {}, path);
  await syncFilesystem(module, true);
}

function displayedPixelSize(canvas) {
  const bounds = canvas.getBoundingClientRect();
  const pixelRatio = Math.max(window.devicePixelRatio || 1, 1);
  return {
    width: Math.max(Math.round(bounds.width * pixelRatio), 1),
    height: Math.max(Math.round(bounds.height * pixelRatio), 1),
    pixelRatio,
  };
}

export async function prepare({ canvas, onResize, onStopped }) {
  const createModule = (await loadModuleFactory()).default;
  const module = await createModule({
    canvas,
    locateFile: (file) => new URL(file, bundleRoot).href,
    print: (line) => console.log(`[awc] ${line}`),
    printErr: (line) => console.error(`[awc] ${line}`),
  });
  await mountStorage(module);

  return () => {
    let resolution = displayedPixelSize(canvas);
    if (!module._native_awc_start(resolution.width, resolution.height)) {
      const message = module.ccall("native_awc_last_error", "string", [], []);
      throw new Error(message || "native runtime initialization failed");
    }

    let running = true;
    let previous = performance.now();
    let suspended = document.hidden;
    let syncPending = false;
    let lastSync = previous;

    const flush = async () => {
      if (syncPending)
        return;
      syncPending = true;
      try {
        await syncFilesystem(module, false);
      } finally {
        syncPending = false;
      }
    };

    const detach = () => {
      resizeObserver.disconnect();
      window.removeEventListener("resize", resize);
      document.removeEventListener("visibilitychange", visibility);
      window.removeEventListener("pagehide", pageHide);
    };

    const resize = () => {
      const next = displayedPixelSize(canvas);
      if (next.width === resolution.width && next.height === resolution.height)
        return;
      if (!module._native_awc_resize(next.width, next.height))
        return;
      resolution = next;
      onResize?.(resolution);
    };

    const resizeObserver = new ResizeObserver(resize);
    resizeObserver.observe(canvas);
    window.addEventListener("resize", resize);

    const frame = (now) => {
      if (!running)
        return;
      if (suspended) {
        previous = now;
        requestAnimationFrame(frame);
        return;
      }
      const elapsed = Math.min((now - previous) / 1000, 0.25);
      previous = now;
      running = Boolean(module._native_awc_frame(elapsed));
      if (now - lastSync >= 750) {
        lastSync = now;
        void flush();
      }
      if (running) {
        requestAnimationFrame(frame);
      } else {
        detach();
        module._native_awc_shutdown();
        void flush();
        onStopped?.();
      }
    };

    const visibility = () => {
      suspended = document.hidden;
      previous = performance.now();
      if (suspended)
        void flush();
    };
    const pageHide = () => void flush();
    document.addEventListener("visibilitychange", visibility);
    window.addEventListener("pagehide", pageHide);
    requestAnimationFrame(frame);

    return {
      resumeAudio() {
        return Boolean(module._native_awc_audio_resume());
      },
      resolution() {
        return resolution;
      },
      async stop() {
        if (!running)
          return;
        running = false;
        module._native_awc_shutdown();
        await flush();
        detach();
      },
    };
  };
}
