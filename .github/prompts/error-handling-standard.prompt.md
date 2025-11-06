---
mode: 'agent'
description: '오류 처리/예외 정책 표준화 (C++)'
---
대상: C++17 이상.

입력(선택):
- 현재 정책 요약: ${input:policy:'예외 사용 여부, 반환 타입 규약 등 (선택)'}
- 선택 코드: ${selection}

작업:
1) 예외 vs 오류코드 경계 정의(도메인/인프라/바운더리 별)
2) 반환 타입 가이드(Result/expected, std::error_code, optional)
3) 로깅 지점/컨텍스트 전파/스택 정보 전략
4) API 서피스의 noexcept/throws 주석 일관화
5) 대표 함수 diff 예시 제공

출력: 통합 가이드라인 문서 초안 + 변경 패치 스케치
