(function () {
  "use strict";

  // ─── DOM refs ────────────────────────────────────────────────────────────────
  const dropzone      = document.getElementById("dropzone");
  const dropzoneLoaded = document.getElementById("dropzone-loaded");
  const dropzoneFile  = document.getElementById("dropzone-filename");
  const dropzoneClear = document.getElementById("dropzone-clear");

  const progressWrap  = document.getElementById("progress-wrap");
  const progressFill  = document.getElementById("progress-fill");
  const progressLabel = document.getElementById("progress-label");

  const stemGrid      = document.getElementById("stem-grid");

  const btnSeparate   = document.getElementById("btn-separate");
  const btnSaveAll    = document.getElementById("btn-save-all");
  const statusLine    = document.getElementById("status-line");

  // Stem card buttons
  const STEMS = ["vocals", "drums", "bass", "other"];
  const playBtns = {}, saveBtns = {};
  STEMS.forEach(s => {
    playBtns[s] = document.getElementById(`play-${s}`);
    saveBtns[s] = document.getElementById(`save-${s}`);
  });

  // ─── Waveform canvas ─────────────────────────────────────────────────────────
  const STEM_COLORS = { vocals: "#FF2D78", drums: "#00F5D4", bass: "#FFE000", other: "#FF6B00" };

  // Per-stem waveform character: [freq1, freq2, freq3], [amp1, amp2, amp3], animSpeed
  const WF_CFG = {
    vocals: { f: [5, 11, 2.5], a: [9, 4, 6],   spd: 1.2 },
    drums:  { f: [11, 22, 5],  a: [13, 5, 3],  spd: 2.2 },
    bass:   { f: [2.5, 5, 1],  a: [13, 6, 8],  spd: 0.6 },
    other:  { f: [7, 15, 3.5], a: [9, 5, 5],   spd: 1.0 },
  };

  const wfPhase  = { vocals: 0, drums: 0, bass: 0, other: 0 };
  const wfAnimId = {};

  function drawWaveform(stem, playing, phase) {
    const canvas = document.getElementById(`wf-${stem}`);
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    const W = canvas.width, H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    const cfg   = WF_CFG[stem];
    const color = STEM_COLORS[stem];

    ctx.beginPath();
    ctx.strokeStyle = playing ? color : "rgba(255,255,255,0.18)";
    ctx.lineWidth   = 1.5;
    ctx.shadowColor = playing ? color : "transparent";
    ctx.shadowBlur  = playing ? 8 : 0;

    const cy = H / 2;
    for (let x = 0; x <= W; x++) {
      const t   = x / W;
      const env = Math.sin(t * Math.PI); // fade at edges
      let y = cy;
      for (let i = 0; i < 3; i++) {
        y += Math.sin(t * Math.PI * cfg.f[i] + phase * (i + 1) * 0.6) * cfg.a[i] * env;
      }
      x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function initWaveforms() {
    STEMS.forEach(stem => {
      const canvas = document.getElementById(`wf-${stem}`);
      if (!canvas) return;
      canvas.width  = canvas.offsetWidth  || 260;
      canvas.height = canvas.offsetHeight || 48;
      drawWaveform(stem, false, 0);
    });
  }

  function startWaveformAnim(stem) {
    stopWaveformAnim(stem);
    const cfg = WF_CFG[stem];
    function frame() {
      wfPhase[stem] += 0.04 * cfg.spd;
      drawWaveform(stem, true, wfPhase[stem]);
      wfAnimId[stem] = requestAnimationFrame(frame);
    }
    wfAnimId[stem] = requestAnimationFrame(frame);
  }

  function stopWaveformAnim(stem) {
    if (wfAnimId[stem]) { cancelAnimationFrame(wfAnimId[stem]); wfAnimId[stem] = null; }
    drawWaveform(stem, false, wfPhase[stem]);
  }

  // ─── State ───────────────────────────────────────────────────────────────────
  let audioInputPath = null;
  let isSeparating   = false;
  let stemPaths      = {};       // { vocals: "/tmp/...", drums: ..., }
  let pollInterval   = null;
  const audioPlayers = {};       // HTMLAudioElement per stem
  let playingStem    = null;

  // ─── Helpers ─────────────────────────────────────────────────────────────────
  function setStatus(msg, active = false) {
    statusLine.textContent = msg;
    statusLine.classList.toggle("is-active", active);
  }

  function setProgress(pct) {
    progressFill.style.width = `${pct}%`;
    progressLabel.textContent = `${pct}%`;
  }

  // ─── Dropzone ────────────────────────────────────────────────────────────────
  function setAudioFile(filePath) {
    audioInputPath = filePath;
    const name = filePath.split("/").pop();
    dropzoneFile.textContent = name;
    dropzone.classList.add("is-loaded");
    btnSeparate.disabled = false;
  }

  function clearAudioFile() {
    audioInputPath = null;
    dropzoneFile.textContent = "";
    dropzone.classList.remove("is-loaded");
    btnSeparate.disabled = true;
  }

  dropzone.addEventListener("click", async (e) => {
    if (e.target === dropzoneClear) return;
    const filePath = await window.electronAPI.openAudioFile();
    if (filePath) setAudioFile(filePath);
  });

  dropzoneClear.addEventListener("click", (e) => {
    e.stopPropagation();
    clearAudioFile();
  });

  dropzone.addEventListener("dragover", (e) => {
    e.preventDefault();
    dropzone.classList.add("drag-over");
  });
  dropzone.addEventListener("dragleave", () => dropzone.classList.remove("drag-over"));
  dropzone.addEventListener("drop", (e) => {
    e.preventDefault();
    dropzone.classList.remove("drag-over");
    const file = e.dataTransfer.files[0];
    if (file && file.path) setAudioFile(file.path);
  });

  // ─── Separation ──────────────────────────────────────────────────────────────
  function startPoll() {
    pollInterval = setInterval(async () => {
      try {
        const snap = await window.electronAPI.getProgress();

        if (snap.status === "separating") {
          setStatus(`Separating… ${snap.percent}%`, true);
          setProgress(snap.percent);

        } else if (snap.status === "done") {
          stopPoll();
          stemPaths = snap.stems;
          setProgress(100);
          onSeparationDone();

        } else if (snap.status === "error") {
          stopPoll();
          setStatus(`Error: ${snap.error_message}`);
          finishSeparating(false);
        }
      } catch (err) {
        console.error("[poll]", err);
      }
    }, 600);
  }

  function stopPoll() {
    if (pollInterval) { clearInterval(pollInterval); pollInterval = null; }
  }

  function finishSeparating(success) {
    isSeparating = false;
    btnSeparate.textContent = "Separate";
    btnSeparate.disabled = !audioInputPath;
    if (!success) {
      progressWrap.style.display = "none";
    }
  }

  function onSeparationDone() {
    finishSeparating(true);
    setStatus("Done — stems ready", true);
    // Fade out progress bar
    setTimeout(() => { progressWrap.style.display = "none"; }, 800);

    // Reveal stem grid
    stemGrid.style.display = "";
    stemGrid.classList.add("is-ready");
    btnSaveAll.disabled = false;

    // Enable stem buttons and wire up audio
    STEMS.forEach(stem => {
      const p = stemPaths[stem];
      playBtns[stem].disabled = !p;
      saveBtns[stem].disabled = !p;
      if (p) {
        // Create audio player — absolute path needs file:/// (three slashes)
        const el = new Audio(`file:///${p.replace(/^\//, "")}`);
        el.addEventListener("ended", () => stopStem(stem));
        audioPlayers[stem] = el;
      }
    });
  }

  btnSeparate.addEventListener("click", async () => {
    if (!audioInputPath) return;

    if (isSeparating) {
      await window.electronAPI.cancel();
      stopPoll();
      progressWrap.style.display = "none";
      setStatus("Cancelled");
      finishSeparating(false);
      return;
    }

    // Reset stem grid
    stemGrid.style.display = "none";
    btnSaveAll.disabled = true;
    stemPaths = {};
    STEMS.forEach(s => {
      playBtns[s].disabled = true;
      saveBtns[s].disabled = true;
      stopStem(s);
    });

    // Start separation
    isSeparating = true;
    btnSeparate.textContent = "Cancel";
    progressWrap.style.display = "flex";
    setProgress(0);
    setStatus("Starting…", true);

    try {
      await window.electronAPI.separate(audioInputPath);
      startPoll();
    } catch (err) {
      console.error("[separate]", err);
      setStatus(`Error: ${err.message}`);
      finishSeparating(false);
    }
  });

  // ─── Stem playback ───────────────────────────────────────────────────────────
  function stopStem(stem) {
    const el = audioPlayers[stem];
    if (el) { el.pause(); el.currentTime = 0; }
    const btn = playBtns[stem];
    if (btn) { btn.textContent = "Play"; btn.classList.remove("is-playing"); }
    document.getElementById(`card-${stem}`)?.classList.remove("is-playing");
    stopWaveformAnim(stem);
    if (playingStem === stem) playingStem = null;
  }

  function stopAllStems() {
    STEMS.forEach(stopStem);
  }

  STEMS.forEach(stem => {
    playBtns[stem].addEventListener("click", () => {
      if (playingStem === stem) {
        stopStem(stem);
        return;
      }
      stopAllStems();
      const el = audioPlayers[stem];
      if (!el) return;
      el.currentTime = 0;
      el.play().catch(err => console.error("[play]", err));
      playBtns[stem].textContent = "Stop";
      playBtns[stem].classList.add("is-playing");
      document.getElementById(`card-${stem}`)?.classList.add("is-playing");
      startWaveformAnim(stem);
      playingStem = stem;
    });

    saveBtns[stem].addEventListener("click", async () => {
      const p = stemPaths[stem];
      if (!p) return;
      const saved = await window.electronAPI.saveStem(p, stem);
      if (saved) setStatus(`Saved: ${saved.split("/").pop()}`, true);
    });
  });

  // ─── Save All ────────────────────────────────────────────────────────────────
  btnSaveAll.addEventListener("click", async () => {
    const dir = await window.electronAPI.saveAllStems(stemPaths);
    if (dir) setStatus(`All stems saved to: ${dir.split("/").pop()}`, true);
  });

  // ─── Init ────────────────────────────────────────────────────────────────────
  initWaveforms();

  window.electronAPI.onSidecarReady(() => {
    setStatus("Ready");
    // Separate button stays disabled until a file is loaded
  });

  document.addEventListener("contextmenu", (e) => e.preventDefault());

})();
