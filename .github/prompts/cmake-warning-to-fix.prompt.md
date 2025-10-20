---
mode: 'agent'
description: 'CMake 경고 정리 및 수정 PR 제안'
---

입력(선택): ${input:log:'cmake 구성/빌드 로그 (선택)'}
작업:
- 루트/서브 CMakeLists.txt, CMakePresets.json의 경고/비권장 사용 스캔
- MSVC(/permissive-, /Zc:__cplusplus, /W4), RTTI/예외 설정 일관성 점검
출력:
1) 경고 요약(위치|메시지|영향)
2) 변경 전/후 패치(diff)
3) 빌드/설치/런타임 영향
