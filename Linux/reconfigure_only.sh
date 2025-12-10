#!/usr/bin/env bash
# CMake 캐시 삭제 후 configure만 수행
# 사용법: script/reconfigure_only.sh
set -euo pipefail

if [ ! -f "CMakeLists.txt" ]; then
  echo "이 스크립트는 프로젝트 루트에서 실행해야 합니다 (CMakeLists.txt 미발견)."
  exit 1
fi

echo "[reconfigure_only] 시작"

if [ -d build ]; then
  echo "- CMake 캐시 삭제: build/CMakeCache.txt, build/CMakeFiles"
  rm -rf build/CMakeCache.txt build/CMakeFiles || true
else
  echo "- build 디렉터리 없음: 생성"
  mkdir -p build
fi

# Configure
# If NDDSHOME is set and callers didn't provide RTIDDSGEN_EXECUTABLE/RTI_LIB_DIR,
# derive sensible defaults so IdlKit CMake can find rtiddsgen on Unix.
if [ -z "${RTIDDSGEN_EXECUTABLE:-}" ] && [ -n "${NDDSHOME:-}" ]; then
  RTIDDSGEN_EXECUTABLE="$NDDSHOME/bin/rtiddsgen"
  echo "- RTIDDSGEN_EXECUTABLE not provided, using: $RTIDDSGEN_EXECUTABLE"
fi
if [ -z "${RTI_LIB_DIR:-}" ] && [ -n "${NDDSHOME:-}" ]; then
  # default chosen based on common Connext install layout; adjust if your install differs
  RTI_LIB_DIR="$NDDSHOME/lib/x64Linux4gcc7.3.0"
  echo "- RTI_LIB_DIR not provided, using: $RTI_LIB_DIR"
fi

cmake -S . -B build -G Ninja \
  ${RTIDDSGEN_EXECUTABLE:+-DRTIDDSGEN_EXECUTABLE=$RTIDDSGEN_EXECUTABLE} \
  ${RTI_LIB_DIR:+-DRTI_LIB_DIR=$RTI_LIB_DIR}

echo "[reconfigure_only] 완료"
