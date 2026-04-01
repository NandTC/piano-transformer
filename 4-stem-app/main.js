/**
 * main.js — Electron main process for 4-Stem
 */

const { app, BrowserWindow, ipcMain, dialog } = require("electron");
app.commandLine.appendSwitch("disable-features", "AudioServiceOutOfProcess,AudioServiceLaunchOnStartup");

const path = require("path");
const fs   = require("fs");
const http = require("http");
const { spawn } = require("child_process");

const WINDOW_WIDTH  = 660;
const WINDOW_HEIGHT = 700;

function getSidecarCommand() {
  if (app.isPackaged) {
    // Packaged: run the PyInstaller-built binary bundled in resources/sidecar/
    const bin = process.platform === "win32" ? "sidecar.exe" : "sidecar";
    return {
      cmd: path.join(process.resourcesPath, "sidecar", bin),
      args: [],
      opts: { stdio: ["ignore", "pipe", "pipe"] },
    };
  }
  // Dev: run via conda python
  return {
    cmd: "/opt/homebrew/Caskroom/miniconda/base/envs/four_stem/bin/python",
    args: [path.join(__dirname, "python", "sidecar.py")],
    opts: {
      env: Object.assign({}, process.env, {
        PYTHONPATH: path.join(__dirname, "python"),
      }),
      stdio: ["ignore", "pipe", "pipe"],
    },
  };
}

// ─── HTTP helpers ─────────────────────────────────────────────────────────────

function httpGet(port, urlPath) {
  return new Promise((resolve, reject) => {
    const req = http.get(`http://127.0.0.1:${port}${urlPath}`, (res) => {
      let data = "";
      res.on("data", (c) => (data += c));
      res.on("end", () => { try { resolve(JSON.parse(data)); } catch (e) { reject(e); } });
    });
    req.on("error", reject);
    req.setTimeout(5000, () => { req.destroy(); reject(new Error("timeout")); });
  });
}

function httpPost(port, urlPath, body) {
  return new Promise((resolve, reject) => {
    const bodyStr = JSON.stringify(body);
    const req = http.request({
      hostname: "127.0.0.1", port, path: urlPath, method: "POST",
      headers: { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(bodyStr) },
    }, (res) => {
      let data = "";
      res.on("data", (c) => (data += c));
      res.on("end", () => { try { resolve(JSON.parse(data)); } catch (e) { reject(e); } });
    });
    req.on("error", reject);
    req.write(bodyStr);
    req.end();
  });
}

// ─── State ────────────────────────────────────────────────────────────────────

let mainWindow    = null;
let sidecarProcess = null;
let sidecarPort   = null;

// ─── Sidecar ─────────────────────────────────────────────────────────────────

function spawnSidecar() {
  const { cmd, args, opts } = getSidecarCommand();
  sidecarProcess = spawn(cmd, args, opts);

  return new Promise((resolve, reject) => {
    let resolved = false;

    sidecarProcess.stdout.on("data", (chunk) => {
      const text = chunk.toString();
      console.log("[sidecar]", text.trim());
      if (!resolved) {
        const m = text.match(/SIDECAR_PORT:(\d+)/);
        if (m) { sidecarPort = parseInt(m[1], 10); resolved = true; resolve(sidecarPort); }
      }
    });

    sidecarProcess.stderr.on("data", (chunk) => {
      const text = chunk.toString();
      console.error("[sidecar-err]", text.trim());
    });

    sidecarProcess.on("error", (err) => { if (!resolved) reject(err); });
    sidecarProcess.on("exit", (code) => {
      console.log("[main] Sidecar exited:", code);
      sidecarProcess = null;
    });

    setTimeout(() => { if (!resolved) reject(new Error("Sidecar port timeout")); }, 30000);
  });
}

// ─── Window ───────────────────────────────────────────────────────────────────

async function createWindow() {
  mainWindow = new BrowserWindow({
    width: WINDOW_WIDTH,
    height: WINDOW_HEIGHT,
    resizable: false,
    titleBarStyle: "hiddenInset",
    backgroundColor: "#FFFFFF",
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.loadFile(path.join(__dirname, "src", "index.html"));
  if (!app.isPackaged) mainWindow.webContents.openDevTools({ mode: "detach" });

  mainWindow.webContents.on("console-message", (_e, level, msg, line, src) => {
    console.log(`[renderer] ${msg}`);
  });
  mainWindow.webContents.on("render-process-gone", (_e, d) => {
    console.log("[renderer CRASHED]", JSON.stringify(d));
  });
  mainWindow.on("closed", () => { mainWindow = null; });
}

// ─── App lifecycle ────────────────────────────────────────────────────────────

app.whenReady().then(async () => {
  try {
    await spawnSidecar();
    console.log("[main] Sidecar port:", sidecarPort);
  } catch (err) {
    console.error("[main] Sidecar spawn failed:", err.message);
  }
  await createWindow();
  mainWindow.webContents.on("did-finish-load", () => {
    mainWindow.webContents.send("sidecar-ready");
  });
});

app.on("window-all-closed", () => {
  if (sidecarProcess) sidecarProcess.kill();
  app.quit();
});

// ─── IPC handlers ─────────────────────────────────────────────────────────────

ipcMain.handle("get-port", () => sidecarPort);

ipcMain.handle("open-audio-file", async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: "Select Audio File",
    filters: [{ name: "Audio Files", extensions: ["mp3", "wav", "flac", "aiff", "m4a", "aac"] }],
    properties: ["openFile"],
  });
  if (result.canceled || result.filePaths.length === 0) return null;
  return result.filePaths[0];
});

ipcMain.handle("separate", async (_e, inputPath) => {
  return httpPost(sidecarPort, "/separate", { inputPath });
});

ipcMain.handle("get-progress", async () => {
  return httpGet(sidecarPort, "/progress");
});

ipcMain.handle("cancel", async () => {
  return httpPost(sidecarPort, "/cancel", {});
});

ipcMain.handle("save-stem", async (_e, stemPath) => {
  const ext  = path.extname(stemPath);
  const base = path.basename(stemPath, ext);
  const result = await dialog.showSaveDialog(mainWindow, {
    title: "Save Stem",
    defaultPath: base,
    filters: [{ name: "WAV Audio", extensions: ["wav"] }],
  });
  if (result.canceled || !result.filePath) return null;
  fs.copyFileSync(stemPath, result.filePath);
  return result.filePath;
});

ipcMain.handle("save-all-stems", async (_e, stems) => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: "Choose Folder to Save All Stems",
    properties: ["openDirectory"],
  });
  if (result.canceled || result.filePaths.length === 0) return null;
  const dir = result.filePaths[0];
  for (const [key, srcPath] of Object.entries(stems)) {
    const dest = path.join(dir, `${key}.wav`);
    fs.copyFileSync(srcPath, dest);
  }
  return dir;
});
