"""
midi_utils.py — MIDI read/write helpers for PianoTransformer sidecar.

Handles:
- Reading a primer .mid file into Magenta NoteSequence
- Encoding NoteSequence to Performance event tokens
- Writing a NoteSequence to .mid with a given tempo
"""

import note_seq
from note_seq import midi_io
from note_seq.protobuf import music_pb2


def read_primer_midi(midi_path, primer_length_tokens):
    """
    Read a MIDI file and encode it as Performance event tokens.
    Returns a list of integer token IDs trimmed to primer_length_tokens.
    Returns empty list if midi_path is None or primer_length_tokens == 0.
    """
    if not midi_path or primer_length_tokens == 0:
        return []

    try:
        ns = note_seq.midi_file_to_note_sequence(midi_path)
    except Exception as e:
        print(f"[midi_utils] Failed to read primer MIDI: {e}")
        return []

    if len(ns.notes) == 0:
        print("[midi_utils] Primer MIDI has no notes — using unconditioned generation")
        return []

    # Encode using Performance encoding (same vocabulary as piano_transformer)
    encoder_decoder = note_seq.OneHotEventSequenceEncoderDecoder(
        note_seq.PerformanceOneHotEncoding(
            num_velocity_bins=32,
            max_shift_steps=100
        )
    )

    performance = note_seq.Performance(
        note_sequence=ns,
        num_velocity_bins=32,
        max_shift_steps=100
    )

    token_ids = encoder_decoder.encode(performance)

    # Trim to primer_length
    trimmed = token_ids[:primer_length_tokens]
    print(f"[midi_utils] Primer encoded: {len(token_ids)} tokens → using {len(trimmed)}")
    return trimmed


def write_midi(note_sequence, output_path, tempo_bpm=120):
    """
    Write a NoteSequence proto to a .mid file at the given tempo.
    """
    # Apply tempo
    if note_sequence.tempos:
        note_sequence.tempos[0].qpm = tempo_bpm
    else:
        tempo = note_sequence.tempos.add()
        tempo.time = 0.0
        tempo.qpm = tempo_bpm

    note_seq.sequence_proto_to_midi_file(note_sequence, output_path)
    print(f"[midi_utils] MIDI written to {output_path} at {tempo_bpm} BPM")
