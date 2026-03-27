"""
sidecar.py — PianoTransformer entry point.

Phase 1 (CLI mode): generates a MIDI file from the command line.
Phase 2 (Server mode): starts a Flask HTTP server for Electron IPC.

Usage (CLI / Phase 1):
  python sidecar.py \
    --checkpoint-dir /path/to/model-checkpoint \
    --temperature 1.0 \
    --sequence-length 512 \
    --primer-length 64 \
    --primer-midi /path/to/seed.mid \   # optional
    --tempo 120 \
    --output output.mid

Usage (Server / Phase 2 — started by Electron):
  python sidecar.py --serve --checkpoint-dir /path/to/model-checkpoint
"""

import argparse
import os
import sys

# Ensure python/ directory is on the path when called from another cwd
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def parse_args():
    p = argparse.ArgumentParser(description="PianoTransformer sidecar")
    p.add_argument("--checkpoint-dir", required=True,
                   help="Path to piano_transformer checkpoint directory")
    p.add_argument("--temperature", type=float, default=1.0,
                   help="Sampling temperature (0.1–2.0)")
    p.add_argument("--sequence-length", type=int, default=512,
                   help="Number of event tokens to generate (128–2048)")
    p.add_argument("--primer-length", type=int, default=0,
                   help="Number of primer tokens to use from seed MIDI (0–512)")
    p.add_argument("--primer-midi", type=str, default=None,
                   help="Path to seed .mid file (optional)")
    p.add_argument("--tempo", type=int, default=120,
                   help="Output MIDI tempo in BPM (40–200)")
    p.add_argument("--output", type=str, default="output.mid",
                   help="Output .mid file path")
    p.add_argument("--serve", action="store_true",
                   help="Run as Flask HTTP server (Phase 2)")
    return p.parse_args()


def run_cli(args):
    """Phase 1: generate MIDI from command line."""
    from midi_utils import read_primer_midi
    from generate import load_model, generate_midi

    print(f"[sidecar] Checkpoint dir: {args.checkpoint_dir}")
    print(f"[sidecar] Temperature: {args.temperature}")
    print(f"[sidecar] Sequence length: {args.sequence_length}")
    print(f"[sidecar] Primer length: {args.primer_length}")
    print(f"[sidecar] Primer MIDI: {args.primer_midi or 'none (unconditioned)'}")
    print(f"[sidecar] Tempo: {args.tempo} BPM")
    print(f"[sidecar] Output: {args.output}")
    print()

    # Load model
    load_model(args.checkpoint_dir)

    # Encode primer (if provided)
    primer_tokens = read_primer_midi(args.primer_midi, args.primer_length)

    # Generate
    output = generate_midi(
        checkpoint_dir=args.checkpoint_dir,
        temperature=args.temperature,
        sequence_length=args.sequence_length,
        primer_tokens=primer_tokens,
        tempo_bpm=args.tempo,
        output_path=args.output,
    )

    print(f"\n[sidecar] ✓ MIDI generated: {output}")


def run_server(args):
    """Phase 2: Flask HTTP server for Electron IPC."""
    # Import here to keep Phase 1 usable without server.py
    from server import start_server
    start_server(checkpoint_dir=args.checkpoint_dir)


if __name__ == "__main__":
    args = parse_args()

    if args.serve:
        run_server(args)
    else:
        run_cli(args)
