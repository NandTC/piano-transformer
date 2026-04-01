"""
separate.py — 4-stem audio separation using Meta Demucs via audio-separator.

Model: htdemucs_ft (4 stems: Vocals, Drums, Bass, Other)
Auto-downloads model on first use (~320 MB to ~/.cache/audio-separator/).
"""

import os
import threading
import tempfile
import logging


# ─── Progress state ───────────────────────────────────────────────────────────

class SeparationProgress:
    def __init__(self):
        self.lock = threading.Lock()
        self.reset()

    def reset(self):
        with self.lock:
            self.status = "idle"    # idle | separating | done | error
            self.percent = 0
            self.stems = {}         # { "vocals": "/path/...", "drums": ..., }
            self.error_message = None

    def set(self, **kwargs):
        with self.lock:
            for k, v in kwargs.items():
                setattr(self, k, v)

    def snapshot(self):
        with self.lock:
            return {
                "status": self.status,
                "percent": self.percent,
                "stems": dict(self.stems),
                "error_message": self.error_message,
            }


progress = SeparationProgress()

MODEL_NAME = "htdemucs_ft.yaml"

# Maps keywords in output filenames → stem keys
STEM_KEYWORDS = {
    "vocals": "vocals",
    "drums":  "drums",
    "bass":   "bass",
    "other":  "other",
    "guitar": "guitar",
    "piano":  "piano",
}


def _stem_key_from_path(path):
    name = os.path.basename(path).lower()
    for keyword, key in STEM_KEYWORDS.items():
        if f"({keyword})" in name or f"_{keyword}_" in name:
            return key
    return None


def separate_audio(input_path):
    """
    Separate input_path into 4 stems using htdemucs_ft.
    Updates progress state throughout. Stores output paths in progress.stems.
    """
    import time

    progress.set(status="separating", percent=5, stems={}, error_message=None)

    try:
        from audio_separator.separator import Separator

        output_dir = tempfile.mkdtemp(prefix="4stem_")

        # Time-based progress estimate (~2-3min for a 3-min song on CPU)
        # We animate 5→90% during separation, jump to 100% when done.
        start_time = time.time()
        estimated_seconds = 180
        stop_progress = threading.Event()

        def _animate():
            while not stop_progress.is_set():
                elapsed = time.time() - start_time
                pct = min(int(5 + (elapsed / estimated_seconds) * 85), 90)
                progress.set(percent=pct)
                stop_progress.wait(timeout=2.0)

        anim_thread = threading.Thread(target=_animate, daemon=True)
        anim_thread.start()

        separator = Separator(
            output_dir=output_dir,
            output_format="WAV",
            log_level=logging.WARNING,
        )
        separator.load_model(MODEL_NAME)
        output_files = separator.separate(input_path)

        stop_progress.set()
        anim_thread.join(timeout=3)

        # Map output filenames to stem keys — ensure absolute paths
        stems = {}
        for f in output_files:
            abs_f = f if os.path.isabs(f) else os.path.join(output_dir, f)
            key = _stem_key_from_path(abs_f)
            if key:
                stems[key] = abs_f

        if not stems:
            raise RuntimeError(f"No stems found in output: {output_files}")

        print(f"[separate] Done — stems: {list(stems.keys())}")
        progress.set(status="done", percent=100, stems=stems)

    except Exception as e:
        stop_progress.set() if "stop_progress" in dir() else None
        msg = str(e)
        print(f"[separate] ERROR: {msg}")
        progress.set(status="error", error_message=msg)
        raise
