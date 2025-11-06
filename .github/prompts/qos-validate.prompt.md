---
mode: 'agent'
description: 'QoS XML 검증 및 적용 포인트'
---

입력(선택): ${input:qos:'qos_profiles.xml/NGVA_QoS_Library.xml 중 일부 (선택)'}
작업:
- 프로파일 상속/오버라이드 관계 표
- Reliability/History/ResourceLimits/Deadline/Liveliness 요약
- Topic/Writer/Reader 바인딩 지점 제안
- 위험 조합 경고와 튜닝 가이드
- 테스트(루프백/지연/패킷로스) 시나리오
