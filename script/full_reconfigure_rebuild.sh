#!/usr/bin/env bash
# CMake 캐시 삭제 -> configure -> clean -> 전체 재빌드
# 사용법: script/full_reconfigure_rebuild.sh [target]
set -euo pipefail

TARGET=${1:-}
CPUS=${NPROC:-$(nproc)}

# 루트 확인
if [ ! -f "CMakeLists.txt" ]; then
  echo "이 스크립트는 프로젝트 루트에서 실행해야 합니다 (CMakeLists.txt 미발견)."
  exit 1
fi

echo "[full_reconfigure_rebuild] 시작"

# CMake 캐시 제거
if [ -d build ]; then
  echo "- CMake 캐시 및 내부 파일 삭제 (build/CMakeCache.txt, build/CMakeFiles)"
  rm -rf build/CMakeCache.txt build/CMakeFiles || true
else
  echo "- build 디렉터리 없음: 새로 생성합니다"
  mkdir -p build
fi

# Configure
echo "- CMake configure 실행"
# provide NDDSHOME-derived defaults for rtiddsgen and RTI lib dir if not explicitly set
if [ -z "${RTIDDSGEN_EXECUTABLE:-}" ] && [ -n "${NDDSHOME:-}" ]; then
  RTIDDSGEN_EXECUTABLE="$NDDSHOME/bin/rtiddsgen"
  echo "- RTIDDSGEN_EXECUTABLE not provided, using: $RTIDDSGEN_EXECUTABLE"
fi
if [ -z "${RTI_LIB_DIR:-}" ] && [ -n "${NDDSHOME:-}" ]; then
  RTI_LIB_DIR="$NDDSHOME/lib/x64Linux4gcc7.3.0"
  echo "- RTI_LIB_DIR not provided, using: $RTI_LIB_DIR"
fi

cmake -S . -B build -G Ninja \
  ${RTIDDSGEN_EXECUTABLE:+-DRTIDDSGEN_EXECUTABLE=$RTIDDSGEN_EXECUTABLE} \
  ${RTI_LIB_DIR:+-DRTI_LIB_DIR=$RTI_LIB_DIR}

# Clean (기존 생성물 제거)
if [ -d build ]; then
  echo "- 프로젝트 clean (cmake --build build --target clean)"
  cmake --build build --target clean || true
fi

# Build
if [ -n "$TARGET" ]; then
  echo "- 타깃 빌드: $TARGET"
  RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} RTIDDSGEN_EXECUTABLE=${RTIDDSGEN_EXECUTABLE:-$NDDSHOME/bin/rtiddsgen} cmake --build build --target "$TARGET" -- -j${CPUS}
else
  echo "- 전체 빌드"
  RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} RTIDDSGEN_EXECUTABLE=${RTIDDSGEN_EXECUTABLE:-$NDDSHOME/bin/rtiddsgen} cmake --build build -- -j${CPUS}
fi

echo "[full_reconfigure_rebuild] 완료"
