const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("electronAPI", {
  onSidecarReady:  (cb) => ipcRenderer.on("sidecar-ready", () => cb()),
  openAudioFile:   ()  => ipcRenderer.invoke("open-audio-file"),
  separate:        (inputPath) => ipcRenderer.invoke("separate", inputPath),
  getProgress:     ()  => ipcRenderer.invoke("get-progress"),
  cancel:          ()  => ipcRenderer.invoke("cancel"),
  saveStem:        (stemPath, stemKey) => ipcRenderer.invoke("save-stem", stemPath, stemKey),
  saveAllStems:    (stems)    => ipcRenderer.invoke("save-all-stems", stems),
});
