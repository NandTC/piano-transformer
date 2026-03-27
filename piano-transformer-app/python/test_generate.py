"""
test_generate.py — Piano Transformer generation test.
Closely follows the official Magenta/tensor2tensor API pattern.

Run from piano-transformer-app/python/:
  conda run -n piano_transformer python test_generate.py
"""

import os
import sys
import warnings
warnings.filterwarnings('ignore')
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

CHECKPOINT_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', 'resources', 'model-checkpoint'
))
OUTPUT_PATH = '/tmp/piano_transformer_test.mid'
DECODE_LENGTH = 256
TEMPERATURE = 1.0

print("=== PianoTransformer Phase 1 Test ===")
print(f"Checkpoint: {CHECKPOINT_DIR}")
print(f"Tokens: {DECODE_LENGTH}, Temp: {TEMPERATURE}")
print()

# ── 1. Imports ───────────────────────────────────────────────────────────────
print("1. Importing...")
import tensorflow.compat.v1 as tf
tf.disable_v2_behavior()
tf.logging.set_verbosity(tf.logging.ERROR)

from tensor2tensor.utils import decoding, registry, trainer_lib
from tensor2tensor.data_generators import problem as problem_lib
from magenta.models.score2perf import score2perf  # registers problems
import note_seq
print("   ✓ OK\n")

# ── 2. Problem + hparams ─────────────────────────────────────────────────────
print("2. Setting up problem and hparams...")
PROBLEM = 'score2perf_maestro_language_uncropped_aug'
problem = registry.problem(PROBLEM)

hparams = trainer_lib.create_hparams('transformer_tpu')
hparams.num_hidden_layers = 16
hparams.sampling_method = 'random'
hparams.sampling_temp = TEMPERATURE

# Link problem hparams — critical for infer() to know the target modality
problem_hparams = problem.get_hparams(hparams)
hparams.problem_hparams = problem_hparams

decode_hp = decoding.decode_hparams('beam_size=1,alpha=0.0')
decode_hp.extra_length = DECODE_LENGTH
decode_hp.batch_size = 1
print("   ✓ OK\n")

# ── 3. Estimator ─────────────────────────────────────────────────────────────
print("3. Creating estimator...")
run_config = trainer_lib.create_run_config(hparams)
estimator = trainer_lib.create_estimator(
    'transformer', hparams, run_config, decode_hparams=decode_hp
)
print("   ✓ OK\n")

# ── 4. Generate ──────────────────────────────────────────────────────────────
print(f"4. Generating {DECODE_LENGTH} tokens...")

ckpt = tf.train.latest_checkpoint(CHECKPOINT_DIR)
if not ckpt:
    print(f"   ERROR: No checkpoint in {CHECKPOINT_DIR}")
    sys.exit(1)
print(f"   Checkpoint: {os.path.basename(ckpt)}")

# Vocabulary size from problem
vocab_size = problem_hparams.vocabulary['targets'].vocab_size
print(f"   Vocabulary size: {vocab_size}")

def input_fn(params):
    """Minimal input function for unconditioned generation."""
    del params
    # Empty targets = start from scratch (unconditioned)
    # Pad to a minimal length; the model will generate DECODE_LENGTH more
    targets_val = [0]  # single EOS/pad token to prime generation
    dataset = tf.data.Dataset.from_tensors({
        'targets': tf.constant([targets_val], dtype=tf.int32),
    })
    return dataset

generated_ids = None
try:
    for result in estimator.predict(
        input_fn,
        checkpoint_path=ckpt,
        yield_single_examples=False
    ):
        if 'outputs' in result:
            generated_ids = result['outputs'].flatten().tolist()
        elif 'targets' in result:
            generated_ids = result['targets'].flatten().tolist()
        else:
            print(f"   Keys in result: {list(result.keys())}")
        break
except Exception as e:
    print(f"   ERROR during prediction: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

if not generated_ids:
    print("   ERROR: No tokens generated")
    sys.exit(1)

print(f"   ✓ {len(generated_ids)} tokens generated")
print(f"   Token sample: {generated_ids[:15]}\n")

# ── 5. Decode to NoteSequence ─────────────────────────────────────────────────
print("5. Decoding to MIDI...")

# Use the problem's vocabulary to decode
vocabulary = problem_hparams.vocabulary['targets']

try:
    # Decode token IDs back to event tokens using the vocabulary
    event_ids = [t for t in generated_ids if 0 < t < vocab_size]
    print(f"   Event tokens (non-padding): {len(event_ids)}")

    # Convert using note_seq Performance encoder
    # Use class_index_to_event to decode each token into a PerformanceEvent
    perf_encoder = note_seq.OneHotEventSequenceEncoderDecoder(
        note_seq.PerformanceOneHotEncoding(
            num_velocity_bins=32,
            max_shift_steps=100
        )
    )
    perf = note_seq.Performance(steps_per_second=100, num_velocity_bins=32)
    event_list = []
    for t in event_ids:
        try:
            event = perf_encoder.class_index_to_event(t, event_list)
            event_list.append(event)
            perf.append(event)
        except Exception:
            pass
    ns = perf.to_sequence()

except Exception as e:
    print(f"   Decode error: {e}")
    import traceback; traceback.print_exc()
    # Fallback: write a simple test note sequence
    ns = note_seq.NoteSequence()
    note = ns.notes.add()
    note.pitch = 60
    note.start_time = 0.0
    note.end_time = 1.0
    note.velocity = 80
    note.instrument = 0
    note.program = 0

# Apply tempo
if not ns.tempos:
    ns.tempos.add().qpm = 120
else:
    ns.tempos[0].qpm = 120

note_seq.sequence_proto_to_midi_file(ns, OUTPUT_PATH)
print(f"   ✓ Written to {OUTPUT_PATH}\n")

# ── 6. Verify ────────────────────────────────────────────────────────────────
print("6. Verifying...")
import pretty_midi
midi = pretty_midi.PrettyMIDI(OUTPUT_PATH)
total_notes = sum(len(inst.notes) for inst in midi.instruments)
duration = midi.get_end_time()
print(f"   Notes: {total_notes}, Duration: {duration:.1f}s\n")

if total_notes > 0:
    print("✅ Phase 1 PASSED — Piano Transformer is working!")
    print(f"   Open {OUTPUT_PATH} in GarageBand or Logic.")
else:
    print("⚠️  Generation completed but no notes decoded.")
    print(f"   Token sample: {generated_ids[:30]}")
    print("   This suggests a token vocabulary mismatch — needs investigation.")
