"""
server.py — Flask HTTP sidecar for PianoTransformer.

Electron spawns this process at launch:
  python sidecar.py --serve --checkpoint-dir <path>

On startup it:
  1. Picks a random free port
  2. Prints "SIDECAR_PORT:<port>" to stdout (Electron reads this)
  3. Loads the model checkpoint (blocks until ready)
  4. Starts Flask on localhost:<port>

Endpoints:
  GET  /health           → {"status": "ready"} once model is loaded
  POST /generate         → start generation (async, returns immediately)
  GET  /progress         → {"status": "...", "percent": 0-100, "output_path": "..."}
  POST /cancel           → cancel in-progress generation
  GET  /version          → {"version": "1.0.0"}
"""

import os
import sys
import socket
import threading
import tempfile

from flask import Flask, jsonify, request

# Ensure python/ directory is on path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate as gen
from midi_utils import read_primer_midi

app = Flask(__name__)

_checkpoint_dir = None
_model_ready = False
_generation_thread = None
_cancel_flag = threading.Event()


# ─────────────────────────────────────────────
# Routes
# ─────────────────────────────────────────────

@app.route("/health")
def health():
    if _model_ready:
        return jsonify({"status": "ready"})
    return jsonify({"status": "loading"}), 503


@app.route("/version")
def version():
    return jsonify({"version": "1.0.0"})


@app.route("/generate", methods=["POST"])
def generate():
    global _generation_thread, _cancel_flag

    if not _model_ready:
        return jsonify({"error": "Model not ready yet"}), 503

    data = request.get_json(force=True) or {}

    temperature = float(data.get("temperature", 1.0))
    sequence_length = int(data.get("sequenceLength", 512))
    primer_length = int(data.get("primerLength", 0))
    primer_midi = data.get("primerMidi")  # absolute path or None
    tempo_bpm = int(data.get("tempo", 120))

    # Clamp values
    temperature = max(0.1, min(2.0, temperature))
    sequence_length = max(128, min(2048, sequence_length))
    primer_length = max(0, min(512, primer_length))
    tempo_bpm = max(40, min(200, tempo_bpm))

    # If generation already in progress, reject
    snap = gen.progress.snapshot()
    if snap["status"] == "generating":
        return jsonify({"error": "Generation already in progress"}), 409

    # Reset state
    gen.progress.reset()
    _cancel_flag.clear()

    # Encode primer
    primer_tokens = read_primer_midi(primer_midi, primer_length) if primer_midi else []

    # Output to temp file
    fd, output_path = tempfile.mkstemp(suffix=".mid", prefix="pt_")
    os.close(fd)

    def _run():
        try:
            gen.generate_midi(
                checkpoint_dir=_checkpoint_dir,
                temperature=temperature,
                sequence_length=sequence_length,
                primer_tokens=primer_tokens,
                tempo_bpm=tempo_bpm,
                output_path=output_path,
            )
        except Exception:
            pass  # error is stored in gen.progress

    _generation_thread = threading.Thread(target=_run, daemon=True)
    _generation_thread.start()

    return jsonify({"status": "started"})


@app.route("/progress")
def progress():
    snap = gen.progress.snapshot()
    return jsonify(snap)


@app.route("/cancel", methods=["POST"])
def cancel():
    _cancel_flag.set()
    gen.progress.set(status="idle", percent=0)
    return jsonify({"status": "cancelled"})


# ─────────────────────────────────────────────
# Startup
# ─────────────────────────────────────────────

def _find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _load_model_async(checkpoint_dir):
    global _model_ready
    try:
        gen.load_model(checkpoint_dir)
        _model_ready = True
        print("[server] Model ready.", flush=True)
    except Exception as e:
        print(f"[server] ERROR loading model: {e}", file=sys.stderr, flush=True)


def start_server(checkpoint_dir):
    global _checkpoint_dir
    _checkpoint_dir = checkpoint_dir

    port = _find_free_port()

    # Print port first so Electron can read it before blocking on model load
    print(f"SIDECAR_PORT:{port}", flush=True)

    # Load model in background thread so Flask starts immediately
    loader = threading.Thread(target=_load_model_async, args=(checkpoint_dir,), daemon=True)
    loader.start()

    print(f"[server] Starting Flask on 127.0.0.1:{port}", flush=True)
    app.run(host="127.0.0.1", port=port, threaded=True)
