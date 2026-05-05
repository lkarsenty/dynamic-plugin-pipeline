#!/usr/bin/env bash
# =============================================================================
# build.sh — Build the main application and all plugin .so files into ./output
# Usage:
#   ./build.sh          # Full build
#   ./build.sh clean    # Clean build artifacts
#   CC=clang ./build.sh # Use an alternative compiler (default: gcc-13, fallback: gcc)
# =============================================================================

set -euo pipefail

# ---------- Colors and logging ----------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
log_status()  { echo -e "${GREEN}[BUILD]${NC} $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC}  $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# ---------- Output directory ----------
OUTDIR="output"

# ---------- Clean option ----------
if [[ "${1:-}" == "clean" ]]; then
  mkdir -p "$OUTDIR"
  rm -rf "$OUTDIR"/*
  log_status "Cleaned $OUTDIR/"
  exit 0
fi

# ---------- Platform check ----------
OS_NAME="$(uname -s)"
if [[ "$OS_NAME" != "Linux" ]]; then
  log_error "This project uses Linux/glibc dynamic loading APIs (dlmopen)."
  log_error "Detected $OS_NAME. Build and test it in Linux or with the Docker workflow in README."
  exit 1
fi

# ---------- Compiler selection ----------
: "${CC:=}"
if [[ -z "$CC" ]]; then
  if command -v gcc-13 >/dev/null 2>&1; then
    CC=gcc-13
  else
    CC=gcc
    log_warn "gcc-13 not found — falling back to gcc"
  fi
fi

# ---------- Compilation flags ----------
CFLAGS="${CFLAGS:--std=c11 -O2 -g -Wall -Wextra -fno-common}"
CFLAGS+=" -D_POSIX_C_SOURCE=200809L"
LDLIBS="${LDLIBS:--ldl -lpthread}"

# ---------- Output directory ----------
mkdir -p "$OUTDIR"

# ---------- Common sources ----------
COMMON_SRCS=(plugins/plugin_common.c plugins/sync/monitor.c plugins/sync/consumer_producer.c)
for f in "${COMMON_SRCS[@]}"; do
  [[ -f "$f" ]] || { log_error "Missing common source: $f"; exit 1; }
done

# ---------- Build main ----------
MAIN_SRC="main.c"
MAIN_BIN="$OUTDIR/analyzer"
[[ -f "$MAIN_SRC" ]] || { log_error "Missing $MAIN_SRC"; exit 1; }

log_status "Building main → $MAIN_BIN"
"$CC" $CFLAGS -o "$MAIN_BIN" "$MAIN_SRC" "${COMMON_SRCS[@]}" $LDLIBS

# ---------- Build plugins ----------
mapfile -t PLUGIN_SRCS < <(find plugins -maxdepth 1 -type f -name "*.c" ! -name "plugin_common.c" | sort || true)

if ((${#PLUGIN_SRCS[@]} == 0)); then
  log_warn "No plugin sources found under plugins/*.c"
else
  log_status "Building plugins (.so)"
  for src in "${PLUGIN_SRCS[@]}"; do
    base=$(basename "$src" .c)
    so="$OUTDIR/$base.so"
    echo "  → $so"
    "$CC" $CFLAGS -fPIC -shared -o "$so" "$src" "${COMMON_SRCS[@]}" $LDLIBS
  done
fi

log_status "Build complete. Artifacts are in $OUTDIR/"
