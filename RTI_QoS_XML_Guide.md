# RTI Connext 7.5.x (Modern C++) — QoS XML 개발 가이드

**환경 가정**
- RTI Connext 설치 경로: `C:\Program Files\rti_connext_dds-7.5.0`
- 타깃: `x64Win64VS2017`
- API: **RTI Modern C++** (`dds::*`, `rti::core::*`)

본 문서는 RTI Connext 7.5.x 기준으로 QoS XML을 작성·적용하는 방법과 정책 태그/값을 요약합니다. 예시는 Modern C++ API 사용을 전제로 합니다.

---

## 1) 개요

- QoS는 **XML 라이브러리/프로파일**로 정의합니다.
- 애플리케이션에서는 `dds::core::QosProvider`(및 `rti::core::QosProviderParams`)를 사용해 XML을 로드하고, `datawriter_qos()` / `datareader_qos()` / `topic_qos()`로 QoS를 가져와 엔티티 생성에 사용합니다.
- XML은 파일(`file://...`)뿐 아니라 **문자열(XML, `str://"<xml>"`)** 소스도 지원합니다.
- 프로파일은 상속(`base_name="Lib::Profile"`)을 통해 다른 프로파일 값을 기반으로 일부만 덮어쓸 수 있습니다.
- 이미 생성된 엔티티의 QoS는 `reload_profiles()`로 바뀌지 않습니다. **새로 생성하는 엔티티부터** 변경된 프로파일이 반영됩니다.

---

## 2) Modern C++에서 XML 로드/사용

### 2.1 파일/문자열 로드 예시
```cpp
#include <dds/core/QosProvider.hpp>
#include <rti/core/QosProvider.hpp>
#include <rti/core/QosProviderParams.hpp>

// 1) 파일 기반 로드
dds::core::QosProvider qp_file(
    "file://C:/Program Files/rti_connext_dds-7.5.0/resource/xml/NDDS_QOS_PROFILES.xml");

// 2) 문자열(XML) 기반 로드: str://"<xml>"
std::string xml = R"(<dds><qos_library name="UserLib">...</qos_library></dds>)";
dds::core::QosProvider qp_str("str://\"" + xml + "\"");

// 3) 고급: 여러 문자열 소스를 파라미터로 연결
dds::core::StringSeq seq; 
seq.push_back(xml);

rti::core::QosProviderParams params; 
params.string_profile(seq);

auto qp_ex = rti::core::create_qos_provider_ex(params);

// 재적용/언로드
qp_file.reload_profiles();
qp_file.unload_profiles();
```

### 2.2 프로파일에서 QoS 취득 및 엔티티 생성
```cpp
#include <dds/domain/DomainParticipant.hpp>
#include <dds/topic/Topic.hpp>
#include <dds/pub/Publisher.hpp>
#include <dds/pub/DataWriter.hpp>

dds::domain::DomainParticipant participant(0);
dds::pub::Publisher pub(participant);
dds::topic::Topic<MyType> topic(participant, "MyTopic");

// 프로파일로 Writer QoS 취득
auto dwq = qp_file.datawriter_qos("NetLib::control_low_latency_reliable");
dds::pub::DataWriter<MyType> writer(pub, topic, dwq);
```

> 팁: 자동 enable이 기본이므로, 생성 전 QoS를 완성한 뒤 엔티티를 생성하는 흐름이 안전합니다. 필요 시 `EntityFactoryQosPolicy.autoenable_created_entities=false`로 끄고 `enable()` 시점을 제어하세요.

---

## 3) QoS XML 태그/값 요약

> XML에서는 DDS 열거형에서 **`DDS_` 접두어를 제거**한 값을 사용합니다. 예: `DDS_RELIABLE_RELIABILITY_QOS` → `RELIABLE_RELIABILITY_QOS`.

아래 표기는 각 정책별 **태그 구조**와 **유효한 값(kind 등)** 을 요약합니다. 태그 위치는 Writer/Reader/Topic 또는 Pub/Sub QoS 영역에 따라 다를 수 있습니다.

### 3.1 Reliability
```xml
<reliability>
  <kind>BEST_EFFORT_RELIABILITY_QOS | RELIABLE_RELIABILITY_QOS</kind>
  <!-- 선택적 Writer 필드 -->
  <max_blocking_time><sec>...</sec><nanosec>...</nanosec></max_blocking_time>
  <acknowledgment_kind>PROTOCOL | APPLICATION_AUTO | APPLICATION_EXPLICIT</acknowledgment_kind>
  <instance_state_consistency_kind>
    NO_RECOVER_INSTANCE_STATE_CONSISTENCY | RECOVER_INSTANCE_STATE_CONSISTENCY
  </instance_state_consistency_kind>
</reliability>
```

### 3.2 Durability
```xml
<durability>
  <kind>
    VOLATILE_DURABILITY_QOS |
    TRANSIENT_LOCAL_DURABILITY_QOS |
    TRANSIENT_DURABILITY_QOS |
    PERSISTENT_DURABILITY_QOS
  </kind>
</durability>
```

### 3.3 Ownership / OwnershipStrength
```xml
<ownership>
  <kind>SHARED_OWNERSHIP_QOS | EXCLUSIVE_OWNERSHIP_QOS</kind>
</ownership>
<!-- EXCLUSIVE일 때 Writer에 설정 -->
<ownership_strength><value>0..2147483647</value></ownership_strength>
```

### 3.4 Presentation  *(Publisher/Subscriber QoS)*
```xml
<presentation>
  <access_scope>
    INSTANCE_PRESENTATION_QOS | TOPIC_PRESENTATION_QOS | GROUP_PRESENTATION_QOS
  </access_scope>
  <coherent_access>true|false</coherent_access>
  <ordered_access>true|false</ordered_access>
</presentation>
```

### 3.5 Partition  *(Publisher/Subscriber QoS)*
```xml
<partition>
  <name>Control</name>
  <name>Data/*</name> <!-- 와일드카드 지원 -->
</partition>
```

### 3.6 History
```xml
<history>
  <kind>KEEP_LAST_HISTORY_QOS | KEEP_ALL_HISTORY_QOS</kind>
  <depth>1..N</depth> <!-- KEEP_LAST일 때 의미 -->
</history>
```

### 3.7 ResourceLimits
```xml
<resource_limits>
  <max_samples>...</max_samples>
  <max_instances>...</max_instances>
  <max_samples_per_instance>...</max_samples_per_instance>
</resource_limits>
```

### 3.8 Deadline
```xml
<deadline>
  <period><sec>..</sec><nanosec>..</nanosec></period>
</deadline>
```

### 3.9 LatencyBudget
```xml
<latency_budget>
  <duration><sec>..</sec><nanosec>..</nanosec></duration>
</latency_budget>
```

### 3.10 Liveliness
```xml
<liveliness>
  <kind>
    AUTOMATIC_LIVELINESS_QOS |
    MANUAL_BY_PARTICIPANT_LIVELINESS_QOS |
    MANUAL_BY_TOPIC_LIVELINESS_QOS
  </kind>
  <lease_duration>
    <sec>.. | DURATION_INFINITE_SEC</sec>
    <nanosec>.. | DURATION_INFINITE_NSEC</nanosec>
  </lease_duration>
</liveliness>
```

> 호환 예: `Liveliness`는 Writer.kind ≥ Reader.kind, Writer의 lease_duration ≤ Reader.  
> 예: `Deadline`은 Reader.period ≥ Writer.period.  
> `Durability`(TRANSIENT/ PERSISTENT 사용)는 보통 Reliability와 함께 사용합니다.  
> `KEEP_ALL`은 depth 무의미, 대신 ResourceLimits로 제한됩니다.

---

## 4) 샘플 XML

### 4.1 저지연-신뢰형 제어
```xml
<dds>
  <qos_library name="NetLib">
    <qos_profile name="control_low_latency_reliable"
                 is_default_qos="true"
                 base_name="NetLib::BaseUdpOnly">
      <datawriter_qos>
        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
        <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>
        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>
        <deadline><period><sec>0</sec><nanosec>100000000</nanosec></period></deadline>   <!-- 100ms -->
        <latency_budget><duration><sec>0</sec><nanosec>3000000</nanosec></duration></latency_budget> <!-- 3ms -->
        <ownership><kind>EXCLUSIVE_OWNERSHIP_QOS</kind></ownership>
        <transport_priority><value>100</value></transport_priority>
      </datawriter_qos>
      <datareader_qos>
        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
        <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>
        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>
      </datareader_qos>
    </qos_profile>
  </qos_library>
</dds>
```

### 4.2 고신뢰 스트림(깊은 히스토리 + 리소스 상한)
```xml
<dds>
  <qos_library name="StreamLib">
    <qos_profile name="reliable_stream_keep_last_32">
      <datawriter_qos>
        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>32</depth></history>
        <resource_limits>
          <max_samples>4096</max_samples>
          <max_instances>1024</max_instances>
          <max_samples_per_instance>64</max_samples_per_instance>
        </resource_limits>
      </datawriter_qos>
      <datareader_qos>
        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>32</depth></history>
        <resource_limits>
          <max_samples>4096</max_samples>
          <max_instances>1024</max_instances>
          <max_samples_per_instance>64</max_samples_per_instance>
        </resource_limits>
      </datareader_qos>
    </qos_profile>
  </qos_library>
</dds>
```

### 4.3 장기 생존 + 수동 라이브리니스
```xml
<dds>
  <qos_library name="MissionLib">
    <qos_profile name="long_lived_manual_liveliness">
      <datawriter_qos>
        <liveliness>
          <kind>MANUAL_BY_PARTICIPANT_LIVELINESS_QOS</kind>
          <lease_duration><sec>5</sec><nanosec>0</nanosec></lease_duration>
        </liveliness>
        <lifespan>
          <duration><sec>3600</sec><nanosec>0</nanosec></duration> <!-- 1시간 -->
        </lifespan>
      </datawriter_qos>
      <datareader_qos>
        <liveliness>
          <kind>MANUAL_BY_PARTICIPANT_LIVELINESS_QOS</kind>
          <lease_duration><sec>10</sec><nanosec>0</nanosec></lease_duration>
        </liveliness>
      </datareader_qos>
    </qos_profile>
  </qos_library>
</dds>
```

---

## 5) Agent(게이트웨이) 적용 팁

- **프로파일 단위 관리**: UI에서 전달된 XML(문자열)을 그대로 수용해 `QosProvider`에 로드하고, 이후 생성 요청에 `lib::profile` 식별자를 사용합니다.
- **핫 리로드**: UI가 QoS를 변경하면 문자열 소스를 업데이트하고 `reload_profiles()`를 호출합니다. 기존 엔티티는 영향 없음.
- **기본 프로파일 전환**: `default_library()` / `default_profile()` 또는 `is_default_qos`를 통해 기본값을 관리합니다.
- **검증**: JSON→XML 변환 시, 정책 제약(예: Deadline/Liveliness 호환, History vs ResourceLimits)을 사전 검증한 뒤 XML을 생성하고, 최종적으로 XML 로더의 검증을 한 번 더 받는 흐름을 권장합니다.

---

## 6) 체크리스트

- [ ] `RELIABLE/BEST_EFFORT` 호환성 확인(Reader가 RELIABLE이면 Writer도 RELIABLE)
- [ ] `Durability(Transient/Persistent)` 사용 시 Reliability 활성화
- [ ] `Deadline`: Reader ≥ Writer
- [ ] `Liveliness`: Writer.kind ≥ Reader.kind, Writer.lease ≤ Reader.lease
- [ ] `History`: KEEP_LAST의 depth와 ResourceLimits의 일관성
- [ ] 프로파일 상속(`base_name`) 및 기본 프로파일(`is_default_qos`) 설정
- [ ] 엔티티 생성 전 QoS 완성(자동 enable 주의)

---

© RTI Connext 7.5.x / Modern C++ 기준. 설치 경로 및 타깃 환경은 상단 “환경 가정” 참조.
