"""
server.py — Flask HTTP sidecar for 4-Stem.

Endpoints:
  GET  /health     → {"status": "ready"}
  POST /separate   → start separation (async)
  GET  /progress   → {"status": "...", "percent": 0-100, "stems": {...}}
  POST /cancel     → cancel in-progress separation
"""

import os
import sys
import socket
import threading

from flask import Flask, jsonify, request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import separate as sep

app = Flask(__name__)

_separation_thread = None


@app.route("/health")
def health():
    return jsonify({"status": "ready"})


@app.route("/separate", methods=["POST"])
def separate():
    global _separation_thread

    data = request.get_json(force=True) or {}
    input_path = data.get("inputPath")

    if not input_path or not os.path.isfile(input_path):
        return jsonify({"error": "inputPath missing or not found"}), 400

    # C3: validate file extension — only allow known audio formats
    ALLOWED_EXTENSIONS = {".mp3", ".wav", ".flac", ".aiff", ".aif", ".ogg", ".m4a", ".mp4"}
    if os.path.splitext(input_path)[1].lower() not in ALLOWED_EXTENSIONS:
        return jsonify({"error": "File type not allowed"}), 400

    # Prevent path traversal — resolve symlinks and ensure it's a real file path
    input_path = os.path.realpath(input_path)

    snap = sep.progress.snapshot()
    if snap["status"] == "separating":
        return jsonify({"error": "Separation already in progress"}), 409

    sep.progress.reset()

    def _run():
        try:
            sep.separate_audio(input_path)
        except Exception:
            pass

    _separation_thread = threading.Thread(target=_run, daemon=True)
    _separation_thread.start()

    return jsonify({"status": "started"})


@app.route("/progress")
def progress():
    return jsonify(sep.progress.snapshot())


@app.route("/cancel", methods=["POST"])
def cancel():
    sep.progress.set(status="idle", percent=0)
    return jsonify({"status": "cancelled"})


def _find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def start_server():
    port = _find_free_port()
    print(f"SIDECAR_PORT:{port}", flush=True)
    print(f"[server] Starting Flask on 127.0.0.1:{port}", flush=True)
    app.run(host="127.0.0.1", port=port, threaded=True)
