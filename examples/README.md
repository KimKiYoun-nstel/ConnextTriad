# Test runner 사용법

이 디렉터리에는 시나리오 기반의 간단한 테스트 러너가 포함되어 있습니다.

파일

- `test_runner.py` : RIPC(CBOR) 기반의 시나리오 실행기
- `scenarios/scenario1.json` : 샘플 시나리오

요구사항

- Python 3.8+
- `cbor2` 패키지

간단 설치 (PowerShell)

```powershell
cd .\examples
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install cbor2
```

사용 예

```powershell
# workspace 루트에서
python .\examples\test_runner.py .\examples\scenarios\scenario1.json --host 127.0.0.1 --port 25000
```

주요 기능

- 시나리오 포맷 검사: `--validate-only` 플래그로 시나리오 파일의 구조만 검사합니다.
- 템플릿 치환: 요청 바디 내 `${var}` 플레이스홀더를 이전에 저장된 변수로 치환합니다.
  - 예: `{"text": "hello ${writer_id}"}`
- 변수 저장(save): 요청 스텝의 `save` 필드로 응답 결과에서 값을 추출하여 다음 스텝에 사용 가능
  - 예: `"save": { "w1": "result.id" }` — 응답의 `result.id`를 `w1`으로 저장

확장 아이디어

- `save`에 표현식 또는 JSONPath 지원
- `expect`에 정규식, 부분 매칭 등 확장
- 병렬/동시 시나리오 실행을 위한 멀티스레드 모드

문제가 있거나 추가 기능을 원하시면 시나리오 예시를 주시면 바로 확장해 드리겠습니다.
