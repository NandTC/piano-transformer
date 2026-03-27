# PianoTransformer — Python Sidecar Setup

## Requirements

- Conda (Miniconda or Anaconda)
- `gsutil` for downloading the model checkpoint (`brew install --cask google-cloud-sdk`)

## Setup (Mac Intel)

```bash
conda create -n piano_transformer python=3.10
conda activate piano_transformer
pip install -r requirements.txt
```

## Setup (Apple Silicon — M1/M2/M3)

```bash
conda create -n piano_transformer python=3.10
conda activate piano_transformer

# Use Apple's TF build instead of standard tensorflow
pip install tensorflow-macos==2.9.0
pip install tensorflow-metal  # GPU acceleration via Metal (optional but recommended)

# Install other deps (exclude tensorflow from requirements.txt)
pip install magenta==2.1.4 note_seq==0.0.5 pretty_midi==0.2.10 flask==2.3.3
```

## Download Model Checkpoint

```bash
# From the piano-transformer-app/ root
./scripts/download-model.sh
```

This downloads ~1.5GB to `resources/model-checkpoint/`. Run once.

## Test (Phase 1 — CLI)

```bash
conda activate piano_transformer
cd piano-transformer-app/python

# Unconditioned generation (cold start)
python sidecar.py \
  --checkpoint-dir ../resources/model-checkpoint \
  --temperature 1.0 \
  --sequence-length 256 \
  --tempo 120 \
  --output test_output.mid

# With a seed MIDI primer
python sidecar.py \
  --checkpoint-dir ../resources/model-checkpoint \
  --temperature 1.0 \
  --sequence-length 512 \
  --primer-midi /path/to/seed.mid \
  --primer-length 64 \
  --tempo 120 \
  --output continuation.mid
```

Expected output:
- First run: model loading takes 15-30 seconds
- Generation: ~60-180 seconds on CPU for 512 tokens
- Output: a valid .mid file you can drag into any DAW

## Phase 1 Test Criteria

- [ ] `python sidecar.py` generates a valid .mid file in under 3 minutes on CPU
- [ ] Generated .mid opens in a DAW and contains recognizable piano notes
- [ ] Primer continuation sounds like it continues from the seed
- [ ] `temperature=0.1` produces noticeably more repetitive output than `temperature=1.8`
- [ ] No TensorFlow import errors or runtime crashes

## Troubleshooting

**`ModuleNotFoundError: No module named 'magenta'`**
→ Make sure you activated the conda env: `conda activate piano_transformer`

**`tensorflow.python.framework.errors_impl.NotFoundError: ... checkpoint`**
→ Run `./scripts/download-model.sh` first

**TF version errors / `tf.compat.v1` missing**
→ Magenta requires TF ≤ 2.12. Do NOT upgrade to TF 2.13+.
→ Check: `python -c "import tensorflow; print(tensorflow.__version__)"`

**Apple Silicon: `Illegal hardware instruction`**
→ You installed `tensorflow` instead of `tensorflow-macos`. Reinstall with the Apple Silicon instructions above.
