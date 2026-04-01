#!/usr/bin/env bash
# setup-signing.sh — One-shot Mac code signing + notarization setup for 4-Stem
# Run from repo root: bash 4-stem-app/scripts/setup-signing.sh
set -e

REPO="NandTC/piano-transformer"
CERT_PASSWORD="4stem-build-$(openssl rand -hex 8)"
CERT_TMP="/tmp/4stem-cert.p12"

echo ""
echo "══════════════════════════════════════════════"
echo "  4-Stem Mac Signing Setup"
echo "══════════════════════════════════════════════"
echo ""

# ── 1. Find Developer ID Application certificate ──────────────────────────────
echo "▸ Looking for Developer ID Application certificate..."
CERT_LIST=$(security find-identity -v -p codesigning 2>/dev/null | grep "Developer ID Application" || true)

if [ -z "$CERT_LIST" ]; then
  echo "✗ No 'Developer ID Application' certificate found in Keychain."
  echo "  Make sure you've downloaded it from developer.apple.com → Certificates."
  exit 1
fi

echo "$CERT_LIST"
echo ""

# If multiple, grab the first one
CERT_NAME=$(echo "$CERT_LIST" | head -1 | sed 's/.*"\(.*\)"/\1/')
echo "▸ Using: $CERT_NAME"
echo ""

# ── 2. Export .p12 ────────────────────────────────────────────────────────────
echo "▸ Exporting certificate (you'll be prompted for your Keychain password)..."
security export \
  -k "$HOME/Library/Keychains/login.keychain-db" \
  -t identities \
  -f pkcs12 \
  -P "$CERT_PASSWORD" \
  -o "$CERT_TMP"

echo "✓ Certificate exported."
echo ""

# ── 3. Base64 encode ─────────────────────────────────────────────────────────
CSC_LINK_B64=$(base64 -i "$CERT_TMP")
rm -f "$CERT_TMP"

# ── 4. Apple ID info ──────────────────────────────────────────────────────────
echo "▸ Notarization — Apple ID info needed:"
echo "  (For app-specific password: appleid.apple.com → Sign-In & Security → App-Specific Passwords)"
echo ""
read -p "  Apple ID email:              " APPLE_ID
read -s -p "  App-specific password:       " APPLE_ID_PASSWORD
echo ""

# Find Team ID from the certificate name  (format: "Name (TEAMID)")
TEAM_ID=$(echo "$CERT_NAME" | grep -oE '\([A-Z0-9]{10}\)' | tr -d '()' || true)
if [ -z "$TEAM_ID" ]; then
  read -p "  Apple Team ID (10 chars):    " TEAM_ID
fi

echo ""
echo "▸ Setting GitHub Secrets on $REPO ..."

gh secret set CSC_LINK             --body "$CSC_LINK_B64"        --repo "$REPO"
gh secret set CSC_KEY_PASSWORD     --body "$CERT_PASSWORD"       --repo "$REPO"
gh secret set APPLE_ID             --body "$APPLE_ID"            --repo "$REPO"
gh secret set APPLE_ID_PASSWORD    --body "$APPLE_ID_PASSWORD"   --repo "$REPO"
gh secret set APPLE_TEAM_ID        --body "$TEAM_ID"             --repo "$REPO"

echo ""
echo "══════════════════════════════════════════════"
echo "  ✓ All secrets set!"
echo "  Now push a tag to trigger a signed build:"
echo ""
echo "    git tag 4stem-v1.0.0"
echo "    git push && git push --tags"
echo "══════════════════════════════════════════════"
echo ""
