#!/usr/bin/env bash
# CMake 변경은 없지만 빌드 산출물 모두 정리 -> 전체 재빌드
# 사용법: script/full_clean_rebuild.sh [target]
set -euo pipefail

TARGET=${1:-}
CPUS=${NPROC:-$(nproc)}

if [ ! -f "CMakeLists.txt" ]; then
  echo "이 스크립트는 프로젝트 루트에서 실행해야 합니다 (CMakeLists.txt 미발견)."
  exit 1
fi

if [ ! -d build ]; then
  echo "build 디렉터리가 없습니다. 먼저 configure를 실행하세요."
  exit 1
fi

echo "[full_clean_rebuild] 시작"

# Build 시스템 clean
echo "- cmake clean 타깃 실행"
cmake --build build --target clean || true

# CMake 캐시와 설정은 그대로 두고 다른 산출물 삭제 (CMakeCache.txt와 CMakeFiles는 보존)
echo "- build 내부 산출물 삭제 (CMake 캐시는 보존)"
shopt -s extglob || true
# 삭제: build/* 중 CMakeCache.txt, CMakeFiles 제외
for p in build/*; do
  base=$(basename "$p")
  if [ "$base" = "CMakeCache.txt" ] || [ "$base" = "CMakeFiles" ]; then
    echo "  - 보존: build/$base"
  else
    echo "  - 삭제: build/$base"
    rm -rf "build/$base"
  fi
done

# Ensure build system (build.ninja) exists; if not, regenerate configure using NDDSHOME defaults if available
if [ ! -f build/build.ninja ] && [ ! -f build/Makefile ]; then
  echo "- 빌드 시스템(build.ninja 또는 Makefile)이 존재하지 않습니다. Configure를 재실행합니다."
  if [ -z "${RTIDDSGEN_EXECUTABLE:-}" ] && [ -n "${NDDSHOME:-}" ]; then
    RTIDDSGEN_EXECUTABLE="$NDDSHOME/bin/rtiddsgen"
    echo "  - RTIDDSGEN_EXECUTABLE not provided, using: $RTIDDSGEN_EXECUTABLE"
  fi
  if [ -z "${RTI_LIB_DIR:-}" ] && [ -n "${NDDSHOME:-}" ]; then
    RTI_LIB_DIR="$NDDSHOME/lib/x64Linux4gcc7.3.0"
    echo "  - RTI_LIB_DIR not provided, using: $RTI_LIB_DIR"
  fi
  cmake -S . -B build -G Ninja \
    ${RTIDDSGEN_EXECUTABLE:+-DRTIDDSGEN_EXECUTABLE=$RTIDDSGEN_EXECUTABLE} \
    ${RTI_LIB_DIR:+-DRTI_LIB_DIR=$RTI_LIB_DIR}
fi

# 재빌드
if [ -n "$TARGET" ]; then
  echo "- 타깃 빌드: $TARGET"
  RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} RTIDDSGEN_EXECUTABLE=${RTIDDSGEN_EXECUTABLE:-$NDDSHOME/bin/rtiddsgen} cmake --build build --target "$TARGET" -- -j${CPUS}
else
  echo "- 전체 빌드"
  RTI_WORKSPACE=${RTI_WORKSPACE:-/tmp/rti_ws_empty} RTIDDSGEN_EXECUTABLE=${RTIDDSGEN_EXECUTABLE:-$NDDSHOME/bin/rtiddsgen} cmake --build build -- -j${CPUS}
fi

echo "[full_clean_rebuild] 완료"
