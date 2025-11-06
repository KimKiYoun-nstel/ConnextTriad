---
mode: 'agent'
description: 'Connext/CMake/VS 빌드 실패 원인 디버그'
---

전제: Connext 7.5.x, Modern C++ API, C++17, Windows/MSVC, CMakePresets.json.
입력:
- 실패 로그: ${input:log:'빌드/링커 오류 로그를 붙여넣어 주세요'}
- 사용 프리셋: ${input:preset:'예: ninja-release 또는 vs2022-x64'}
수행:
1) CMake preset/generator/toolset 확인
2) NDDSHOME 경로/버전(7.5.x)/권한 검증
3) RTIDDSGEN_EXECUTABLE 경고/오류 경로 검증
4) nddscpp2/nddsc/nddscore 링크와 x64 아키텍처 일치 확인
5) rtiddsgen.bat 실행 가능 여부(공백 경로, 권한, Python3)
6) 첫 에러부터 역추적, 재현 커맨드 제시
출력: 표(검증 명령 | 예상 원인 | 수정 방법) + 재현 명령 블록
