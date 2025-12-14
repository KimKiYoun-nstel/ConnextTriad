#pragma once
/**
 * @file stats_manager.hpp
 * @brief 통계 수집/스케줄러(분단위) 싱글톤
 *
 * 간단 텍스트 형태의 통계 스냅샷을 매분 00초에 콘솔과 선택적으로 파일로 출력합니다.
 */

#include <string>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdint>

namespace rtpdds {

struct StatsSnapshot {
    std::string timestamp; // ISO-ish
    uint64_t ipc_in = 0;
    uint64_t ipc_out = 0;
    size_t participants = 0;
    size_t publishers = 0;
    size_t subscribers = 0;
    size_t writers = 0;
    size_t readers = 0;
    size_t topics = 0;
    std::unordered_map<std::string, uint64_t> writer_counts;
    std::unordered_map<std::string, uint64_t> reader_counts;
    // 현재 매칭된 엔드포인트 수 (Writer/Reader 당 현재 matched count)
    std::unordered_map<std::string, uint32_t> writer_matched;
    std::unordered_map<std::string, uint32_t> reader_matched;
};

class StatsManager {
public:
    static StatsManager& instance();

    // 초기화: 파일 출력 관련
    void init(const std::string& file_dir, const std::string& file_name, bool file_output);

    // 스케줄러 제어
    void start();
    void stop();

    // IPC 카운터
    void inc_ipc_in();
    void inc_ipc_out();

    // DDS 메시지 카운터 (토픽별)
    void inc_writer_count(const std::string& topic);
    void inc_reader_count(const std::string& topic);

    // 현재 매칭(connected) 카운트 설정 (Writer/Reader의 status 콜백에서 호출)
    void set_writer_matched_count(const std::string& topic, uint32_t count);
    void set_reader_matched_count(const std::string& topic, uint32_t count);

    // 설정 출력 포맷 ("text", "csv", "json")
    void set_output_format(const std::string& fmt);

    // 엔티티 상태 스냅샷 설정 (DdsManager에서 호출)
    void set_entity_snapshot(size_t participants, size_t publishers, size_t subscribers, size_t writers, size_t readers, size_t topics);

    // 즉시 스냅샷 획득(테스트용)
    StatsSnapshot snapshot_and_reset_counts();

private:
    StatsManager();
    ~StatsManager();
    StatsManager(const StatsManager&) = delete;
    StatsManager& operator=(const StatsManager&) = delete;

    void scheduler_thread();
    void output_snapshot(const StatsSnapshot& s);

    std::atomic<uint64_t> ipc_in_ {0};
    std::atomic<uint64_t> ipc_out_ {0};

    // topic -> count, protected by mutex for insertion; values are uint64_t
    std::mutex writer_mutex_;
    std::unordered_map<std::string, uint64_t> writer_counts_;
    std::unordered_map<std::string, uint32_t> writer_matched_;

    std::mutex reader_mutex_;
    std::unordered_map<std::string, uint64_t> reader_counts_;
    std::unordered_map<std::string, uint32_t> reader_matched_;

    // entity snapshot (current state)
    std::mutex entity_mutex_;
    size_t participants_ = 0;
    size_t publishers_ = 0;
    size_t subscribers_ = 0;
    size_t writers_ = 0;
    size_t readers_ = 0;
    size_t topics_ = 0;

    bool file_output_ = false;
    std::string file_path_;
    enum class OutputFormat { Text, CSV, JSON };
    OutputFormat format_ = OutputFormat::Text;

    

    std::thread thread_;
    std::atomic<bool> running_ {false};
};

} // namespace rtpdds
