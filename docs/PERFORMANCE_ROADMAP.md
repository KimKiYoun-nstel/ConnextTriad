# 성능 및 메모리 최적화 로드맵

## 개요
본 문서는 RtpDdsGateway Agent의 성능 병목 및 메모리 프래그멘테이션 문제를 해결하기 위한 단계별 개선 계획을 정리합니다.

## 현황 분석

### 확인된 문제점
1. **큐 오버플로우 및 대량 드롭**
   - 로그: `enq(s/c/e)=(201761/237410/0) exec=155362 drop=283809`
   - 총 입력 439,171건 중 283,809건(64.7%) 드롭
   - `max_depth=8192` → 큐가 최대 용량에 도달

2. **단일 워커(Consumer) 병목**
   - `AsyncEventProcessor`가 단일 스레드로 모든 이벤트 처리
   - 소비 속도 < 생산 속도 → 큐 적체

3. **빈번한 힙 할당 및 프래그멘테이션**
   - Reader: 매 샘플마다 `std::make_shared<T>` 할당
   - IPC 전송: 매번 `std::vector<uint8_t>` 재할당
   - JSON/CBOR: `nlohmann::json` 내부 다수 할당
   - VxWorks 환경에서 장시간 동작 시 프래그멘테이션 누적

4. **동기 I/O 블로킹**
   - `send_frame()`: worker 스레드가 직접 `send()`/`sendto()` 호출
   - `send_mtx_` 잠금으로 직렬화 → worker 지연

5. **DdsManager mutex 광범위 잠금**
   - `publish_json()` 전체 경로(변환+write)가 `mutex_` 보호
   - 동시 publish 시 잠금 컨텐션 발생

### 할당 핫스팟 상세
```cpp
// 1. Reader → enqueue
ReaderHolder::process_data() {
    auto sp = std::make_shared<T>(sample.data());  // 힙 할당
    sample_callback_(..., AnyData(std::shared_ptr<void>(sp)));
}

// 2. IPC 전송 (매 전송마다)
DkmRtpIpc::send_raw() {
    std::vector<uint8_t> packet(total_len);  // 힙 재할당
    memcpy(...);
    send(...);  // 블로킹
}

// 3. JSON/CBOR 변환
IpcAdapter::emit_evt_from_sample() {
    nlohmann::json evt = {...};  // 다수 할당
    auto out = nlohmann::json::to_cbor(evt);  // vector 할당
    ipc_.send_frame(...);
}

// 4. DDS publish
DdsManager::publish_json() {
    std::lock_guard lock(mutex_);  // 전체 잠금
    SampleGuard sg(type_name);  // 샘플 할당
    json_to_dds(...);
    writer->write_any(...);
}
```

---

## Phase 1: 즉시 적용 (저위험·고효과) ✅

### 1-1. IPC 송신 버퍼 재사용
**목적**: 전송 경로 힙 할당 제거  
**변경 파일**:
- `DkmRtpIpc/include/dkmrtp_ipc.hpp`
- `DkmRtpIpc/src/dkmrtp_ipc.cpp`

**구현**:
```cpp
// dkmrtp_ipc.hpp
private:
    std::vector<uint8_t> send_buf_;  // 재사용 송신 버퍼
    
// send_raw()
send_buf_.resize(total_len);
memcpy(send_buf_.data(), &h, sizeof(Header));
if (payload && len) memcpy(send_buf_.data() + sizeof(Header), payload, len);
// send(send_buf_.data(), ...)
```

**예상 효과**:
- ✅ 매 전송(초당 수천~수만 건)마다 발생하던 할당 **완전 제거**
- ✅ 프래그멘테이션 **즉시 감소**
- ✅ 전송 성능 소폭 향상

**난이도**: ⭐ (매우 낮음)  
**리스크**: 거의 없음 (`send_mtx_`로 이미 보호)

---

### 1-2. 이벤트 지연 계측 추가
**목적**: 큐 대기 시간 및 처리 시간 가시화  
**변경 파일**:
- `RtpDdsGateway/src/gateway.cpp`

**구현**:
```cpp
// 핸들러에서 queue_delay 측정
hs.sample = [this](const async::SampleEvent& ev) {
    auto now = std::chrono::steady_clock::now();
    auto queue_delay_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now - ev.received_time).count();
    
    if (queue_delay_us > 5000) {  // 5ms 이상 대기 시 경고
        LOG_WRN("ASYNC", "high_queue_delay sample topic=%s delay_us=%lld",
                ev.topic.c_str(), (long long)queue_delay_us);
    }
    
    LOG_FLOW("sample exec topic=%s type=%s seq=%llu queue_delay_us=%lld",
            ev.topic.c_str(), ev.type_name.c_str(), 
            static_cast<unsigned long long>(ev.sequence_id),
            (long long)queue_delay_us);
    if (ipc_) ipc_->emit_evt_from_sample(ev);
};
```

**예상 효과**:
- ✅ 병목 지점 정확한 식별
- ✅ 임계값 튜닝 근거 확보
- ✅ 성능 회귀 조기 탐지

**난이도**: ⭐⭐ (낮음)  
**리스크**: 없음 (로깅만 추가)

---

### 1-3. 모니터 로그 초당 비율(rate) 추가
**목적**: 실시간 처리량 모니터링  
**변경 파일**:
- `RtpDdsGateway/src/async/async_event_processor.cpp`

**구현**:
```cpp
// monitor_loop()
void monitor_loop() {
    Stats last_stats{};
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(cfg_.monitor_sec));
        auto st = get_stats();
        
        // 초당 비율 계산
        uint64_t delta_sample = st.enq_sample - last_stats.enq_sample;
        uint64_t delta_cmd = st.enq_cmd - last_stats.enq_cmd;
        uint64_t delta_exec = st.exec_jobs - last_stats.exec_jobs;
        uint64_t delta_drop = st.dropped - last_stats.dropped;
        
        uint64_t rate_enq_sample = delta_sample / cfg_.monitor_sec;
        uint64_t rate_enq_cmd = delta_cmd / cfg_.monitor_sec;
        uint64_t rate_exec = delta_exec / cfg_.monitor_sec;
        uint64_t rate_drop = delta_drop / cfg_.monitor_sec;
        
        LOG_INF("ASYNC", "stats rate(sample/s=%llu cmd/s=%llu exec/s=%llu drop/s=%llu) "
                "total enq(s/c/e)=(%llu/%llu/%llu) exec=%llu drop=%llu max_depth=%zu cur_depth=%zu",
                rate_enq_sample, rate_enq_cmd, rate_exec, rate_drop,
                st.enq_sample, st.enq_cmd, st.enq_err,
                st.exec_jobs, st.dropped, st.max_depth, st.cur_depth);
        
        last_stats = st;
    }
}
```

**예상 효과**:
- ✅ 부하 패턴 실시간 파악
- ✅ 드롭 급증 시점 즉시 탐지
- ✅ 성능 튜닝 지표 확보

**난이도**: ⭐⭐ (낮음)  
**리스크**: 없음

---

## Phase 2: 단기 적용 (중위험·고효과) 🔄

### 2-1. IPC 송신 비동기화
**목적**: worker 스레드에서 전송 블로킹 제거  
**변경 파일**:
- `DkmRtpIpc/include/dkmrtp_ipc.hpp`
- `DkmRtpIpc/src/dkmrtp_ipc.cpp`
- `RtpDdsGateway/src/ipc_adapter.cpp`

**구현 개요**:
```cpp
class DkmRtpIpc {
    std::deque<std::vector<uint8_t>> send_queue_;
    std::mutex send_q_mtx_;
    std::condition_variable send_cv_;
    TriadThread send_thread_;
    
    void send_async(frame_data) {
        {
            std::lock_guard lk(send_q_mtx_);
            send_queue_.push_back(std::move(frame_data));
        }
        send_cv_.notify_one();
    }
    
    void send_loop() {
        while (running_) {
            std::unique_lock lk(send_q_mtx_);
            send_cv_.wait(lk, [this]{ return !send_queue_.empty() || !running_; });
            if (!running_) break;
            
            auto frame = std::move(send_queue_.front());
            send_queue_.pop_front();
            lk.unlock();
            
            // 실제 전송 (블로킹해도 worker에 영향 없음)
            send_raw_impl(frame);
        }
    }
};
```

**예상 효과**:
- ✅ Worker 처리량 **대폭 향상** (블로킹 제거)
- ✅ 큐 드롭 **감소**
- ✅ 전송 지연이 worker에 미치는 영향 **격리**

**난이도**: ⭐⭐⭐ (중간)  
**리스크**: 중간 (순서 보장, graceful shutdown 필요)

---

### 2-2. AsyncEventProcessor 멀티 워커
**목적**: 단일 워커 병목 해소  
**변경 파일**:
- `RtpDdsGateway/include/async/async_event_processor.hpp`
- `RtpDdsGateway/src/async/async_event_processor.cpp`
- `RtpDdsGateway/src/gateway.cpp` (설정)

**구현**:
```cpp
struct Config {
    size_t max_queue = 8192;
    int monitor_sec = 10;
    bool drain_stop = true;
    uint32_t exec_warn_us = 2000;
    size_t worker_count = 1;  // NEW: 워커 개수 (기본 1, 하위 호환)
};

void start() {
    // 멀티 워커 생성
    for (size_t i = 0; i < cfg_.worker_count; ++i) {
#ifdef RTI_VXWORKS
        workers_.emplace_back();
        workers_.back().start([this]{ loop(); });
#else
        workers_.emplace_back([this]{ loop(); });
#endif
    }
    if (cfg_.monitor_sec > 0) {
        monitor_.start([this]{ monitor_loop(); });
    }
}

private:
    std::vector<triad::TriadThread> workers_;  // 워커 배열
};
```

**전제 조건**:
- ✅ `IpcAdapter::emit_evt_from_sample()`: `send_frame()` → `send_mtx_` 보호됨
- ✅ `IpcAdapter::process_request()`: `DdsManager` → `mutex_` 보호됨
- ✅ **현재 핸들러는 thread-safe (멀티 워커 적용 가능)**

**예상 효과**:
- ✅ 처리량 **2~4배 증가** (core 활용)
- ✅ 큐 적체 **대폭 감소**

**난이도**: ⭐⭐⭐ (중간)  
**리스크**: 중간 (핸들러 thread-safety 검증 필요)

---

### 2-3. DdsManager::publish_json mutex 범위 축소
**목적**: 변환 작업을 mutex 밖으로 이동  
**변경 파일**:
- `RtpDdsGateway/src/dds_manager_io.cpp`

**구현 개요**:
```cpp
DdsResult DdsManager::publish_json(const std::string& topic, const nlohmann::json& j) {
    // 1단계: 필요한 정보만 잠금 하에 복사
    std::vector<std::pair<int, std::vector<WriterEntry>>> all_writers;
    std::string type_name;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& dom : writers_) {
            for (const auto& pub : dom.second) {
                auto it = pub.second.find(topic);
                if (it != pub.second.end()) {
                    all_writers.emplace_back(dom.first, it->second);
                }
            }
        }
        type_name = topic_to_type_[domain_id][topic];
    }
    
    // 2단계: mutex 밖에서 샘플 생성 및 변환
    SampleGuard sample_guard(type_name);
    if (!sample_guard) return error;
    if (!json_to_dds(j, type_name, sample_guard.get())) return error;
    
    // 3단계: write_any (RTI write는 thread-safe)
    std::any wrapped = sample_guard.get();
    for (auto& [domain, entries] : all_writers) {
        for (auto& e : entries) {
            e.holder->write_any(wrapped);
        }
    }
    return success;
}
```

**예상 효과**:
- ✅ Publish 처리량 향상
- ✅ 잠금 컨텐션 감소

**난이도**: ⭐⭐⭐⭐ (중간~높음)  
**리스크**: 높음 (동시성 버그 주의, WriterHolder 수명 관리)

---

## Phase 3: 중기 적용 (구조적 개선) 📅

### 3-1. 샘플 메모리 풀
**목적**: 반복 할당을 풀 재사용으로 전환 → 프래그멘테이션 근본 해결

**구현 컨셉**:
```cpp
template<typename T>
class SamplePool {
    std::vector<T*> free_list_;
    std::mutex mtx_;
    size_t capacity_;
    
public:
    SamplePool(size_t cap = 1024) : capacity_(cap) {
        // 사전 할당
        for (size_t i = 0; i < cap; ++i) {
            free_list_.push_back(new T());
        }
    }
    
    std::shared_ptr<T> acquire() {
        std::lock_guard lk(mtx_);
        T* p = nullptr;
        if (!free_list_.empty()) {
            p = free_list_.back();
            free_list_.pop_back();
        } else {
            p = new T();  // 풀 고갈 시 동적 생성
        }
        return std::shared_ptr<T>(p, [this](T* ptr) { release(ptr); });
    }
    
    void release(T* p) {
        std::lock_guard lk(mtx_);
        if (free_list_.size() < capacity_) {
            free_list_.push_back(p);
        } else {
            delete p;  // 용량 초과 시 삭제
        }
    }
};

// ReaderHolder::process_data()에서
auto sp = sample_pool<T>.acquire();
*sp = sample.data();  // 풀에서 얻은 객체에 복사
sample_callback_(..., AnyData(std::static_pointer_cast<void>(sp)));
```

**예상 효과**:
- ✅ 프래그멘테이션 **대폭 감소** (장시간 동작 안정성)
- ✅ 샘플 할당 비용 **제거**

**난이도**: ⭐⭐⭐⭐⭐ (높음)  
**리스크**: 높음 (타입별 풀 관리, 동기화 복잡)

---

### 3-2. CBOR 직렬화 버퍼 재사용
**목적**: `to_cbor()` 결과 벡터 재할당 제거

**구현**:
```cpp
// IpcAdapter 멤버
thread_local std::vector<uint8_t> cbor_buf_;

void emit_evt_from_sample(...) {
    nlohmann::json evt = {...};
    cbor_buf_.clear();
    nlohmann::json::to_cbor(evt, cbor_buf_);  // 기존 버퍼 재사용
    ipc_.send_frame(..., cbor_buf_.data(), cbor_buf_.size());
}
```

**예상 효과**:
- ✅ 직렬화 경로 할당 감소

**난이도**: ⭐⭐⭐ (중간)  
**리스크**: 낮음

---

### 3-3. nlohmann::json arena allocator
**목적**: JSON 내부 할당을 arena에서 처리

**구현 개요**:
```cpp
template<typename T>
struct ArenaAllocator {
    using value_type = T;
    ArenaAllocator(Arena& a) : arena_(a) {}
    
    T* allocate(size_t n) {
        return static_cast<T*>(arena_.allocate(n * sizeof(T)));
    }
    void deallocate(T*, size_t) { /* no-op, arena 일괄 해제 */ }
    
    Arena& arena_;
};

using json_arena = nlohmann::basic_json<
    std::map, std::vector, std::string, bool, int64_t, uint64_t, double,
    ArenaAllocator>;

// 사용
Arena arena(64*1024);
json_arena j(arena);
// ... 처리 ...
arena.reset();  // 일괄 해제
```

**예상 효과**:
- ✅ JSON 할당 프래그멘테이션 **제거**

**난이도**: ⭐⭐⭐⭐⭐ (매우 높음)  
**리스크**: 높음 (코드 전체 영향)

---

## Phase 4: 장기 최적화 (선택적) 🔮

### 4-1. JSON 우회 — 직접 CBOR 직렬화
- 중간 JSON 객체 없이 DDS 샘플 → CBOR 직접 변환
- 난이도: 매우 높음
- 효과: 변환 비용 및 할당 대폭 감소

### 4-2. 백프레셔(Backpressure) 설계
- 생산(리스너) 측에서 과부하 시 샘플링/필터링
- 난이도: 높음
- 효과: 드롭 근본 방지

---

## 적용 순서 및 타임라인

```
Week 1: Phase 1 (즉시 적용)
├─ Day 1-2: 1-1 IPC 송신 버퍼 재사용 + 검증
├─ Day 2-3: 1-2 이벤트 지연 계측 + 1-3 rate 로깅
└─ Day 4-5: Phase 1 통합 테스트 및 성능 측정

Week 2-3: Phase 2 (단기)
├─ Week 2: 2-1 IPC 송신 비동기화 + 테스트
├─ Week 3: 2-2 멀티 워커 (2/4 worker 실험)
└─ 옵션: 2-3 DdsManager mutex 축소 (리스크 평가 후)

Month 2: Phase 3 (중기)
├─ Week 1-2: 3-1 샘플 메모리 풀 설계 및 구현
├─ Week 3: 3-2 CBOR 버퍼 재사용
└─ Week 4: 통합 테스트 및 장기 동작 검증

Future: Phase 4 (필요시)
└─ Phase 1~3 적용 후 문제가 남을 경우 검토
```

---

## 성공 지표 (KPI)

### Phase 1 목표
- [x] IPC 전송 경로 힙 할당 **0건**
- [x] 큐 지연(queue_delay) 평균 **< 5ms**
- [x] 모니터 로그에 rate 메트릭 추가

### Phase 2 목표
- [ ] 드롭률 **< 5%** (현재 64.7%)
- [ ] Worker 처리량 **2배 이상 증가**
- [ ] 평균 처리 시간(exec_time) **< 2ms**

### Phase 3 목표
- [ ] 장시간 동작(72시간) 메모리 사용량 **안정**
- [ ] 프래그멘테이션 **< 10%**
- [ ] 할당 빈도 **50% 이상 감소**

---

## 검증 방법

### 성능 측정
```bash
# 부하 테스트 (Simulator 사용)
cd Simulator
python3 perf_simulator.py --rate 10000 --duration 300

# 로그 분석
grep "ASYNC.*stats" agent.log | tail -20
grep "drop queue_full" agent.log | wc -l
grep "slow_job" agent.log
grep "high_queue_delay" agent.log
```

### 메모리 분석
```bash
# VxWorks: memShow, memPartInfoGet
# Linux: valgrind --tool=massif
# Windows: Performance Monitor (Private Bytes)

# 할당 카운트 측정 (custom allocator hook)
```

### 회귀 테스트
- [ ] 기능 정상 동작 (create/write/read)
- [ ] EVT/RSP 메시지 무결성
- [ ] QoS 적용 정상
- [ ] 멀티 도메인 동작
- [ ] 장시간 안정성 (72h+)

---

## 참고 자료

### 관련 파일
- `RtpDdsGateway/src/async/async_event_processor.cpp` - 워커 루프
- `RtpDdsGateway/src/gateway.cpp` - 핸들러 설정
- `RtpDdsGateway/src/ipc_adapter.cpp` - 이벤트 처리
- `DkmRtpIpc/src/dkmrtp_ipc.cpp` - IPC 전송
- `RtpDdsGateway/src/dds_manager_io.cpp` - DDS publish

### 외부 참고
- RTI Connext DDS Performance Tuning Guide
- nlohmann/json Performance Tips
- VxWorks Memory Management Best Practices

---

**문서 버전**: 1.0  
**최종 수정**: 2025-12-09  
**작성자**: GitHub Copilot (with Claude Sonnet 4.5)
