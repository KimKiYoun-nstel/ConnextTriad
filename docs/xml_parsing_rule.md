# XML 파싱 규칙 (xml_parsing_rule.md)

## 1. Primitive 타입

- XML: `<member name="A_level" type="int32"/>`
- 코드:
  - `XField.name = "A_level"`
  - `XField.kind = "int32"`
  - `XField.nestedType = ""`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = false`

## 2. String 타입

- XML: `<member name="A_text" type="string" stringMaxLength="255"/>`
- 코드:
  - `XField.name = "A_text"`
  - `XField.kind = "string"`
  - `XField.nestedType = ""`
  - `XField.maxLen = 255`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = false`

## 3. Enum 타입

- XML: `<member name="A_state" type="nonBasic" nonBasicTypeName="P_Alarms_PSM::T_Actual_Alarm_StateType"/>`
- 코드:
  - `XField.name = "A_state"`
  - `XField.kind = "enum"`
  - `XField.nestedType = "P_Alarms_PSM::T_Actual_Alarm_StateType"`
  - `XField.enumVals = [열거값 목록]`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = false`

## 4. Struct/복합 타입

- XML: `<member name="A_sourceID" type="nonBasic" nonBasicTypeName="P_LDM_Common::T_IdentifierType"/>`
- 코드:
  - `XField.name = "A_sourceID"`
  - `XField.kind = "struct"`
  - `XField.nestedType = "P_LDM_Common::T_IdentifierType"`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = false`

## 5. Sequence(배열) 타입

### 5-1. 프리미티브/문자열 원소

- XML: `<member name="A_values" type="nonBasic" nonBasicTypeName="int32" sequenceMaxLength="100"/>`
- 코드:
  - `XField.name = "A_values"`
  - `XField.kind = "sequence"`
  - `XField.sequenceElementType = "int32"`
  - `XField.nestedType = ""`
  - `XField.upperBound = 100`
  - `XField.isSequenceOfPrimitive = true`
  - `XField.isSequenceOfString = false`

### 5-2. 구조체 원소

- XML: `<member name="A_items" type="nonBasic" nonBasicTypeName="P_LDM_Common::T_IdentifierType" sequenceMaxLength="100"/>`
- 코드:
  - `XField.name = "A_items"`
  - `XField.kind = "sequence"`
  - `XField.sequenceElementType = "P_LDM_Common::T_IdentifierType"`
  - `XField.nestedType = "P_LDM_Common::T_IdentifierType"`
  - `XField.upperBound = 100`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = false`

### 5-3. 문자열(char 시퀀스)

- XML: `<member name="A_text" type="nonBasic" nonBasicTypeName="char" sequenceMaxLength="255"/>`
- 코드:
  - `XField.name = "A_text"`
  - `XField.kind = "sequence"`
  - `XField.sequenceElementType = "char"`
  - `XField.nestedType = ""`
  - `XField.upperBound = 255`
  - `XField.isSequenceOfPrimitive = false`
  - `XField.isSequenceOfString = true`

## 6. Typedef

- XML: `<typedef name="T_Int32" type="int32"/>`
- 코드:
  - `XTypeSchema.name = "T_Int32"`
  - `XTypeSchema.fields[0].kind = "int32"`
  - 기타 typedef 케이스는 실제 타입에 따라 위 기준 적용

## 7. 기타 속성

- `key`, `isKey` 등은 `XField.isKey`에 할당

---

## 파싱 코드 분기/우선순위

1. **type 속성**
   - "string", "int32", "float64" 등: `kind`에 그대로 할당, `nestedType`은 빈 문자열
   - "nonBasic": 복합/사용자정의 타입 → 실제 타입 정보(`nonBasicTypeName`, `sequenceMaxLength`)에 따라 분기

2. **nonBasicTypeName 속성**
   - 복합 타입, enum, 시퀀스 등에서 실제 타입명을 `nestedType` 또는 `sequenceElementType`에 할당

3. **stringMaxLength, sequenceMaxLength**
   - 해당 속성이 있으면 `maxLen`, `upperBound`에 할당

4. **enum, struct, typedef 등 복합 타입**
   - `kind`는 "enum", "struct", "sequence" 등으로 명확히 구분
   - `nestedType`에 실제 타입명

5. **시퀀스 타입의 원소 구분**
   - 프리미티브/문자열: `nestedType = ""`, `sequenceElementType`에 타입명
   - 구조체: `nestedType = sequenceElementType = 구조체 타입명`

6. **기타 속성**
   - `key`, `isKey` 등은 별도 변수에 할당

---

이 기준대로 파싱 코드를 작성하면 XML 구조와 의미에 따라 모든 타입/필드 케이스를 일관성 있게 처리할 수 있습니다.
