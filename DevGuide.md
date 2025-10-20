좋아. 네가 올린 `qos_store.cpp` 기준으로 **내장 프로파일 로딩 에러**(builtin://…)를 없애고, **RTI 공식 방식(기본 QosProvider + 내장 프로파일 이름 문자열)**으로 바꾸는 패치 가이드를 정리해줄게. 아래 변경만 하면 끝난다.

(참고: 내가 인용하는 함수/구조 이름은 네 파일 그대로다. 예: `detail_profiles()`, `list_profiles()` 등. )

---

# 변경 요약

1. **헤더 보강**: 내장 프로파일 이름 헬퍼를 쓰려면 집합 헤더를 추가
2. **list_profiles()**: 내장 후보 이름을 **하드코딩 문자열** 대신 **RTI 헬퍼**가 반환하는 문자열로 채우기
3. **detail_profiles()**: `dds::core::QosProvider builtin("builtin://...")` → **`dds::core::QosProvider::Default()`** 로 바꾸고, 반환된 **이름 문자열**로 QoS 조회
4. (선택) `g_missing_builtin_libs` 는 **라이브러리 단위 캐시** 그대로 둬도 되고, **프로파일 단위 캐시**로 바꿔도 된다

아래에 “어디에 무엇을” 정확히 적는다.

---

## 1) 헤더 추가

파일 상단 include 구역에 **하나만** 추가해줘.

```cpp
#include <rti/core/rticore.hpp>   // 내장 프로파일 이름 헬퍼들 포함
```

> 이미 `#include <dds/dds.hpp>` 가 간접 포괄되니 추가 include는 이것만이면 충분. 

---

## 2) list_profiles(bool) 수정

현재 구현은 내장 후보를 아래처럼 **하드코딩 문자열**로 넣고 있어.

```cpp
// 기존 (발췌)
if (include_builtin) {
    static const char* kLibs[]  = {"BuiltinQosLibExp", "BuiltinQosLib"};
    static const char* kProfs[] = {"Generic.StrictReliable", "Generic.BestEffort",
                                   "Baseline.Compatibility", "Baseline.Throughput", "Baseline.Latency"};
    for (auto lib : kLibs) for (auto prof : kProfs)
        out.push_back(std::string(lib) + "::" + prof);
}
```

→ **RTI 헬퍼 함수**로 교체해서 “공식 이름 문자열”을 써:

```cpp
if (include_builtin) {
    using rti::core::builtin_profiles::qos_lib;

    // RTI 공식 제공: 이름 문자열을 돌려주는 헬퍼들
    const std::vector<std::string> candidates = {
        qos_lib::generic_strict_reliable(),   // "BuiltinQosLib::Generic.StrictReliable"
        qos_lib::generic_best_effort(),       // "BuiltinQosLib::Generic.BestEffort"
        qos_lib::baseline(),                  // "BuiltinQosLib::Baseline" (설치/버전에 따라 유효)
        // 필요 시 추가: qos_lib::baseline_throughput(), qos_lib::baseline_latency(), ...
        // 주의: Exp/Snippet 계열은 설치 구성에 따라 없을 수 있음
    };
    out.insert(out.end(), candidates.begin(), candidates.end());
}
```

> 이렇게 하면 나중에 RTI가 이름을 바꿔도(헬퍼가 업데이트), 코드가 안전해. 정렬·중복 제거는 **너 코드가 이미 아래에서 하고 있으니 그대로 둬**. 

---

## 3) detail_profiles(bool) 핵심 수정

문제의 원인이 여기. 현재는 **builtin URI를 QosProvider 생성자에 전달**하고 있어 RTI 내부에서 에러가 터진다.

### 3-1. 기존(문제 코드) — 제거 대상

```cpp
dds::core::QosProvider builtin(std::string("builtin://") + lib);
for (auto prof : kProfs) {
    std::string full = std::string(lib) + "::" + prof;
    if (fill_effective(builtin, full, obj)) { ... }
}
```

↑ 이 블록 전체를 **없애고**, 아래 “정석 방식”으로 교체.

### 3-2. 교체 — 정석(공식) 방식

* **어떤 QosProvider든**(여기선 `Default()`) **내장 프로파일 이름 문자열**을 넘겨 QoS를 얻는다.
* 존재하지 않으면 예외 → **자연스럽게 skip**.

```cpp
if (include_builtin) {
    try {
        auto prov = dds::core::QosProvider::Default();

        using rti::core::builtin_profiles::qos_lib;
        const std::vector<std::string> candidates = {
            qos_lib::generic_strict_reliable(),
            qos_lib::generic_best_effort(),
            qos_lib::baseline(),                // 설치 구성 따라 유효
            // 필요 시 추가 …
        };

        for (const auto& full : candidates) {
            nlohmann::json obj;
            if (fill_effective(prov, full, obj)) {
                obj["source_kind"] = "builtin";
                detail[full] = std::move(obj);
                LOG_DBG("DDS", "[qos-detail] loaded builtin profile=%s via Default()", full.c_str());
            } else {
                LOG_DBG("DDS", "[qos-detail] builtin profile not present: %s", full.c_str());
            }
        }
    } catch (const std::exception& ex) {
        LOG_WRN("DDS", "[qos-detail] builtin profiles query failed: %s", ex.what());
    }
}
```

> 포인트는 **`QosProvider::Default()` + “이름 문자열”** 조합이라는 것. URI로 여는 게 아님.
> `fill_effective()` 는 네가 이미 쓰는 헬퍼라 그대로 재사용하면 된다. 

### 3-3. (선택) 실패 캐시 전략 바꾸기

* 네 파일엔 `g_missing_builtin_libs`(라이브러리 단위 캐시)가 있는데, **이제 라이브러리를 열지 않으니** 필요 없다.
* 대신 **프로파일 문자열 단위**로 실패를 캐시하고 싶다면 간단히 set 하나 둘 수 있어:

```cpp
static std::unordered_set<std::string> g_missing_builtin_profiles;

// 루프 안에서
if (!fill_effective(prov, full, obj)) {
    g_missing_builtin_profiles.insert(full);
    continue;
}
```

> 없어도 무방. RTI 쪽 에러 로그는 `Default()` + 이름 조회에선 거의 나오지 않는다.

---

## 4) `summarize_*()` 는 그대로 OK

* 너 파일의 summarize 구현은 이미 **ISO C++ PSM 규약대로 `policy<T>()`**를 쓰고 있고,
* enum 매핑도 **switch 없이 if/else**라 **빌드 안전**해. 손댈 필요 없다. 

(추후 `presentation/partition` 값을 채우고 싶으면 `publisher_qos(full) / subscriber_qos(full)`도 뽑아 `Presentation/Partition`을 세팅하는 보강 버전으로 확장하면 된다.)

---

## 5) 최종 점검 체크리스트

* [ ] `#include <rti/core/rticore.hpp>` 추가
* [ ] `list_profiles()` 내장 후보를 **헬퍼 반환 문자열**로 구성
* [ ] `detail_profiles()`에서 **builtin:// 제거**, `QosProvider::Default()` + **이름 문자열**로 조회
* [ ] (선택) 내장 실패 캐시 로직 정리(프로파일 단위 or 제거)
* [ ] 나머지 로직(외부 XML 처리, 정렬/중복 제거, 마지막 로드 우선, 로그)은 그대로 유지

---

## 참고: 바꿔야 하는 위치(파일 내부)

* 함수 시그니처/이름은 그대로 두고, **본문만** 교체하면 된다.

  * `std::vector<std::string> QosStore::list_profiles(bool)` — 내장 후보 작성부(하드코딩 문자열 → 헬퍼) 
  * `nlohmann::json QosStore::detail_profiles(bool)` — **builtin 블록** 전체 교체(URI 제거 → Default + 이름) 

이렇게 바꾸면 `builtin://BuiltinQosLib(Exp)` 관련 RTI 에러 로그가 사라지고, **존재하는 내장 프로파일만** 자연스럽게 detail에 포함된다.
