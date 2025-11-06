맞아, **`from_string()`는 Modern C++ `dds::core::QosProvider`에 없는 멤버**라서 C2039가 뜨는 게 정상이야. 7.5.x(현행 문서 기준)에서 XML 문자열을 쓰는 방법은 아래 둘 중 하나야.

---

## 1) `QosProvider` 생성자에 **URI**로 넣기

`QosProvider`는 “리소스 URI”를 받는데, 이 URI는 **파일 경로**뿐 아니라 **문자열 XML**도 받을 수 있어. 문자열은 `str://"<XML문서>"` 형식으로 넘긴다. 문서에 “URI는 파일 또는 `str://"..."`
이라고 명시돼 있어. 그리고 필요 시 `reload_profiles()`로 다시 읽을 수 있어. ([community.rti.com][1])

```cpp
#include <dds/core/QosProvider.hpp>

std::string xml = R"(
<dds>
  <qos_library name="UserLib">
    <qos_profile name="UserProfile" is_default_qos="true">
      <datawriter_qos>
        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>10</depth></history>
      </datawriter_qos>
    </qos_profile>
  </qos_library>
</dds>
)";

// 따옴표 포함해서 str://"<xml>" 형식으로 전달
dds::core::QosProvider qp("str://\"" + xml + "\"");

// 이후:
auto dwq = qp.datawriter_qos("UserLib::UserProfile");
```

필요하면:

```cpp
qp.reload_profiles();   // XML 재적용
qp.unload_profiles();   // 언로드
```

이 함수들은 QosProvider의 공개 멤버로 제공된다. ([community.rti.com][1])

---

## 2) `rti::core::QosProviderParams`에 **string_profile**로 넣고 만들기

문자열(들)을 **프로바이더 파라미터**에 실어 만든 다음, `create_qos_provider_ex()`로 프로바이더를 생성하는 방식도 있다. `string_profile()`은 “XML 문서를 담은 문자열 시퀀스(연결하면 유효한 XML)”를 받는다. ([community.rti.com][2])

```cpp
#include <rti/core/QosProvider.hpp>
#include <rti/core/QosProviderParams.hpp>

dds::core::StringSeq xml_seq;
xml_seq.push_back(xml);

rti::core::QosProviderParams params;
params.string_profile(xml_seq);

auto qp = rti::core::create_qos_provider_ex(params); // 확장 함수
auto dwq = qp.datawriter_qos("UserLib::UserProfile");
```

`create_qos_provider_ex()`와 `QosProviderParams`는 Modern C++ API의 확장(extensions)으로 문서에 명시돼 있다. ([community.rti.com][1])

---

## 참고 팁

* 이미 만든 엔티티의 QoS는 **리로드해도 바뀌지 않음**. 이후에 생성되는 엔티티부터 반영. (프로바이더가 제공하는 것은 *생성용 템플릿* 개념) ([community.rti.com][1])
* 기본 프로바이더를 커스터마이즈하고 싶으면 `default_qos_provider_params()`를 써서 **Default()**가 로드할 리소스를 바꿀 수 있어. ([community.rti.com][1])

요약하면: `from_string()`은 애초에 없고, **URI의 `str://`**를 쓰거나 **`QosProviderParams::string_profile()`+`create_qos_provider_ex()`**를 쓰면 된다. 필요한 쪽으로 바로 코드 맞춰줄까?

[1]: https://community.rti.com/static/documentation/connext-dds/current///doc/api/connext_dds/api_cpp2/classdds_1_1core_1_1QosProvider.html "RTI Connext Modern C++ API: dds::core::QosProvider Class Reference"
[2]: https://community.rti.com/static/documentation/connext-dds/current/doc/api/connext_dds/api_cpp2/classrti_1_1core_1_1QosProviderParams.html "RTI Connext Modern C++ API: rti::core::QosProviderParams Class Reference"
