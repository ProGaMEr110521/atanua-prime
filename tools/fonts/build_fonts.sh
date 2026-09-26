#!/bin/sh
# Rebuild the chrome fonts in data/fonts from upstream Inter and JetBrains
# Mono (both SIL OFL 1.1).
#
# Inter gets its ss04 feature (disambiguation: slab I, tailed l, flagged 1,
# without the slashed zero) frozen into the default glyphs, since Dear
# ImGui does not shape text. That keeps chip names like "Il1" or "74LS01"
# unambiguous. Numeric readouts use JetBrains Mono instead of Inter's tnum
# (whose tabular hyphen breaks names like "3-in"), and Inter 4's smcp only
# covers a few letters, so section labels are uppercased in code instead.
# Everything is subset to Latin, Latin-1, Latin Extended-A, Cyrillic,
# general punctuation and arrows.
#
# Needs: fonttools (pyftsubset), opentype-feature-freezer (pyftfeatfreeze).
# Sources default to the Debian/Ubuntu fonts-inter + fonts-jetbrains-mono
# packages; override INTER_DIR / JBM_DIR to point elsewhere.
set -e
INTER_DIR=${INTER_DIR:-/usr/share/fonts/opentype/inter}
JBM_DIR=${JBM_DIR:-/usr/share/fonts/truetype/jetbrains-mono}
OUT=$(cd "$(dirname "$0")/../../data/fonts" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

UNI="U+0020-007E,U+00A0-00FF,U+0100-017F,U+0400-045F,U+2010-2027,U+2030-203A,U+2190-21FF,U+2212,U+2318,U+2325,U+21E7"

freeze_subset() { # src features out
    pyftfeatfreeze -f "$2" -n "$1" "$TMP/frozen.otf" >/dev/null
    pyftsubset "$TMP/frozen.otf" --unicodes="$UNI" --layout-features='kern' \
        --name-IDs='*' --output-file="$3"
}

freeze_subset "$INTER_DIR/Inter-Regular.otf" "ss04" "$OUT/Inter-Regular.otf"
freeze_subset "$INTER_DIR/Inter-SemiBold.otf" "ss04" "$OUT/Inter-SemiBold.otf"
pyftsubset "$JBM_DIR/JetBrainsMono-Regular.ttf" --unicodes="$UNI" --layout-features='kern' \
    --output-file="$OUT/JetBrainsMono-Regular.ttf"
ls -l "$OUT"
