#!/usr/bin/env bash
# 프로젝트 클린 스크립트
# 사용법: script/clean_project.sh [--all]
#   --all 옵션을 주면 build 디렉터리 전체를 삭제합니다 (캐시 포함).
set -euo pipefail

ALL=false
if [ "${1:-}" = "--all" ]; then
  ALL=true
fi

if [ ! -f "CMakeLists.txt" ]; then
  echo "이 스크립트는 프로젝트 루트에서 실행해야 합니다 (CMakeLists.txt 미발견)."
  exit 1
fi

if [ "$ALL" = true ]; then
  echo "[clean_project] 전체 삭제: build 디렉터리 제거"
  rm -rf build
  echo "- build 디렉터리 삭제됨"
  exit 0
fi

if [ ! -d build ]; then
  echo "build 디렉터리가 없습니다. 정리할 것이 없습니다."
  exit 0
fi

echo "[clean_project] cmake clean 타겟 실행"
cmake --build build --target clean || true

# IDL 생성물 등 추가로 삭제할 항목
if [ -d build/idlkit/gen ]; then
  echo "- IDL 생성물 제거: build/idlkit/gen"
  rm -rf build/idlkit/gen
fi

# 기타 임시/캐시 파일은 남겨둡니다 (원하면 --all을 사용하세요)
echo "[clean_project] 완료 (추가 삭제가 필요하면 --all 옵션으로 전체 삭제)"
