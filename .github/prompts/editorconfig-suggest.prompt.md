---
mode: 'agent'
description: '.editorconfig 제안 생성기 (C++)'
---
입력(선택):
- 선호 규칙: ${input:prefs:'탭/스페이스, 인덴트, 줄바꿈, 인코딩, 줄 길이 등 (선택)'}
- 샘플 코드: ${selection}

작업:
1) 현재 코드 스타일 감지
2) 언어별 섹션(C/C++/CMake/Markdown 등) 포함한 .editorconfig 초안 생성
3) 충돌 가능 지점(기존 formatter/clang-format) 메모

출력: 제안 .editorconfig 본문 + 병행 사용 시 주의사항
