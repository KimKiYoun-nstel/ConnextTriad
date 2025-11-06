---
mode: 'agent'
description: 'IDL 변경 후 재생성 체크리스트'
---

입력(선택): ${input:idl:'변경된 IDL 조각 (선택)'}
체크:
- 대상: IdlKit/idl/*.idl, rtiddsgen.bat, CMake 타깃 DdsTypes
- 산출물: <build>/idlkit/gen/*.hpp, *.cxx, *Plugin.*, *Support.*
- 규칙: *.hpp 참조 OK / *.cxx, Plugin, Support 수정·커밋 금지
- 호환성: 필드/타입/옵션 변화, sequence 길이
- 빌드: cmake --build --preset <preset> --target DdsTypes
- 다운스트림: type_registry, sample_factory 등록
출력: (문제 징후 | 복구 방법) 표
