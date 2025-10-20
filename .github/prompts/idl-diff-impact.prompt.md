---
mode: 'agent'
description: 'IDL 변경 영향(직렬화/QoS/롤아웃) 분석'
---

입력: ${input:diff:'IDL 변경 diff'}
출력:
- ABI/직렬화 영향(Connext 7.5.x 전제)
- 구독/발행 QoS/필터 영향
- sample_factory/JSON 변환기 수정 포인트
- 롤아웃 전략(동시 vs 단계적) 및 리스크
