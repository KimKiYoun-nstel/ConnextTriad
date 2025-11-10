#!/usr/bin/env bash
# 코드 변경만 있을 때 빠르게 빌드
# 사용법: script/incremental_build.sh [target]
set -euo pipefail

TARGET=${1:-}
CPUS=${NPROC:-$(nproc)}

if [ ! -f "CMakeLists.txt" ]; then
  echo "이 스크립트는 프로젝트 루트에서 실행해야 합니다 (CMakeLists.txt 미발견)."
  exit 1
fi

if [ ! -d build ]; then
  echo "build 디렉터리가 없습니다. 먼저 configure를 실행하세요. 예: ./script/reconfigure_only.sh"
  exit 1
fi

if [ -n "$TARGET" ]; then
  echo "[incremental_build] 타깃 빌드: $TARGET"
  cmake --build build --target "$TARGET" -- -j${CPUS}
else
  echo "[incremental_build] 전체 빌드 (변경된 타깃만 컴파일)"
  cmake --build build -- -j${CPUS}
fi

echo "[incremental_build] 완료"
