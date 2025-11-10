#!/usr/bin/env bash
# Preset 기반 빌드 스크립트
# 사용법:
#   ./script/preset_build.sh [--mode debug|release] [--action configure|reconfigure|clean|build|full-rebuild] [--target <target>]
# 기본: mode=debug, action=build
# 이 스크립트는 CMakePresets.json에 정의된 linux-ninja-debug / linux-ninja-release 프리셋을 사용합니다.

set -euo pipefail

MODE="debug"
ACTION="build"
TARGET=""
CPUS=${NPROC:-$(nproc)}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      MODE="$2"; shift 2 ;;
    --action)
      ACTION="$2"; shift 2 ;;
    --target)
      TARGET="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [--mode debug|release] [--action configure|reconfigure|clean|build|full-rebuild] [--target <target>]";
      exit 0 ;;
    *)
      echo "Unknown arg: $1"; exit 1 ;;
  esac
done

if [ "$MODE" != "debug" ] && [ "$MODE" != "release" ]; then
  echo "Invalid mode: $MODE (use debug or release)"; exit 1
fi

PRESET="linux-ninja-${MODE}"
BINARY_DIR="${PWD}/out/build/${PRESET}"

echo "[preset_build] preset=$PRESET binary_dir=$BINARY_DIR action=$ACTION target=${TARGET:-(none)}"

# helper: configure using preset
do_configure() {
  echo "- Configuring with preset: $PRESET"
  cmake --preset "$PRESET"
}

# helper: build
do_build() {
  if [ -n "$TARGET" ]; then
    echo "- Building target: $TARGET"
    RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} cmake --build "$BINARY_DIR" --target "$TARGET" -- -j${CPUS}
  else
    echo "- Building all targets"
    RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} cmake --build "$BINARY_DIR" -- -j${CPUS}
  fi
}

# helper: clean
do_clean() {
  if [ -d "$BINARY_DIR" ]; then
    echo "- Running clean in $BINARY_DIR"
    cmake --build "$BINARY_DIR" --target clean || true
  else
    echo "- Nothing to clean (binary dir not found: $BINARY_DIR)"
  fi
}

# helper: full-rebuild
do_full_rebuild() {
  echo "- Performing full rebuild: remove $BINARY_DIR, configure, build"
  rm -rf "$BINARY_DIR"
  do_configure
  do_build
}

# helper: reconfigure detection (source newer than cache)
needs_reconfigure() {
  if [ ! -f "$BINARY_DIR/CMakeCache.txt" ]; then
    return 0
  fi
  # if any CMakeLists.txt or CMakePresets.json is newer than cache, reconfigure
  src_mtime=$(stat -c %Y "CMakeLists.txt" || echo 0)
  preset_mtime=$(stat -c %Y "CMakePresets.json" || echo 0)
  cache_mtime=$(stat -c %Y "$BINARY_DIR/CMakeCache.txt" || echo 0)
  if [ "$src_mtime" -gt "$cache_mtime" ] || [ "$preset_mtime" -gt "$cache_mtime" ]; then
    return 0
  fi
  # also check any top-level CMake include files (optional)
  return 1
}

# Ensure CMakePresets.json contains the preset (best-effort check)
if ! grep -q "\"name\": \"${PRESET}\"" CMakePresets.json 2>/dev/null; then
  echo "Warning: preset '$PRESET' not found in CMakePresets.json. Ensure presets exist or run configure manually."
fi

case "$ACTION" in
  configure)
    do_configure
    ;;
  reconfigure)
    if needs_reconfigure; then
      echo "- Changes detected: reconfiguring"
      do_configure
    else
      echo "- No configure needed"
    fi
    ;;
  build)
    if needs_reconfigure; then
      echo "- Source/preset changed; running configure first"
      do_configure
    fi
    do_build
    ;;
  clean)
    do_clean
    ;;
  full-rebuild)
    do_full_rebuild
    ;;
  *)
    echo "Unknown action: $ACTION"; exit 1 ;;
esac

echo "[preset_build] done"
