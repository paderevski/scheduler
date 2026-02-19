#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_IMAGE="${1:-$ROOT_DIR/include/logo.jpeg}"
OUTPUT_ICNS="${2:-$ROOT_DIR/resources/ClickSort.icns}"

if [[ ! -f "$SOURCE_IMAGE" ]]; then
  echo "Source image not found: $SOURCE_IMAGE" >&2
  exit 1
fi

if ! command -v sips >/dev/null 2>&1; then
  echo "Missing required tool: sips" >&2
  exit 1
fi

if ! command -v iconutil >/dev/null 2>&1; then
  echo "Missing required tool: iconutil" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
ICONSET_DIR="$TMP_DIR/ClickSort.iconset"
mkdir -p "$ICONSET_DIR"

sizes=(16 32 128 256 512)
for size in "${sizes[@]}"; do
  sips -s format png -z "$size" "$size" "$SOURCE_IMAGE" \
    --out "$ICONSET_DIR/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -s format png -z "$double" "$double" "$SOURCE_IMAGE" \
    --out "$ICONSET_DIR/icon_${size}x${size}@2x.png" >/dev/null
done

mkdir -p "$(dirname "$OUTPUT_ICNS")"
iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"

rm -rf "$TMP_DIR"

echo "Wrote $OUTPUT_ICNS"