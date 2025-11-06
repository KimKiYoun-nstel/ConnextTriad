---
mode: 'agent'
description: '스레드 안전성 점검(경쟁/교착/재진입) — C++'
---
입력:
- 대상 코드: ${selection}
- 동시성 모델 힌트: ${input:model:'락/락프리/액터/스케줄러 등 (선택)'}

작업:
1) 경쟁 조건/ABA/재진입 위험 식별
2) 락 범위/순서/소유권 점검, 교착 가능성 분석
3) 메모리 모델/atomic 사용 적절성 검토
4) 완화책(strand/dispatcher/큐잉, RAII 락, lock_guard/unique_lock, try-lock 백오프)
5) 테스트 전략(경합 스트레스/페어라이즈/TSAN 등)

출력: (이슈 | 근거 | 수정 제안 | 테스트) 표
