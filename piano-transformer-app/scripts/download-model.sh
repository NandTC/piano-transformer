#!/bin/bash
# download-model.sh — Download the Piano Transformer checkpoint from Google Cloud Storage.
#
# Requirements: gsutil (from google-cloud-sdk)
#   Install: brew install --cask google-cloud-sdk
#   Or: pip install gsutil
#
# The checkpoint is ~1.5GB. Run once before building or developing.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODEL_DIR="$SCRIPT_DIR/../resources/model-checkpoint"

mkdir -p "$MODEL_DIR"

echo "Downloading Piano Transformer checkpoint (~1.5GB)..."
echo "Destination: $MODEL_DIR"
echo ""

# Unconditional model (no primer needed — cold generation)
gsutil -m cp \
  "gs://magentadata/models/music_transformer/checkpoints/unconditional_model_16.ckpt.data-00000-of-00001" \
  "gs://magentadata/models/music_transformer/checkpoints/unconditional_model_16.ckpt.index" \
  "gs://magentadata/models/music_transformer/checkpoints/unconditional_model_16.ckpt.meta" \
  "$MODEL_DIR/"

# Write checkpoint file so TF can find it
echo 'model_checkpoint_path: "unconditional_model_16.ckpt"
all_model_checkpoint_paths: "unconditional_model_16.ckpt"' > "$MODEL_DIR/checkpoint"

echo ""
echo "✓ Checkpoint downloaded to: $MODEL_DIR"
echo ""
echo "Files:"
ls -lh "$MODEL_DIR"
