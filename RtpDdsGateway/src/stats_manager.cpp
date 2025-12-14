#include "stats_manager.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>
#include <iostream>
#include <nlohmann/json.hpp>

namespace rtpdds {

StatsManager& StatsManager::instance()
{
    static StatsManager inst;
    return inst;
}

StatsManager::StatsManager() {}
StatsManager::~StatsManager() { stop(); }

void StatsManager::init(const std::string& file_dir, const std::string& file_name, bool file_output)
{
    file_output_ = file_output;
    if (file_output_) {
        // simple path join
        file_path_ = file_dir + "/" + file_name;
    }
}

void StatsManager::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    thread_ = std::thread(&StatsManager::scheduler_thread, this);
}

void StatsManager::stop()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void StatsManager::inc_ipc_in() { ipc_in_.fetch_add(1, std::memory_order_relaxed); }
void StatsManager::inc_ipc_out() { ipc_out_.fetch_add(1, std::memory_order_relaxed); }

void StatsManager::inc_writer_count(const std::string& topic)
{
    std::lock_guard<std::mutex> lk(writer_mutex_);
    writer_counts_[topic]++;
}

void StatsManager::inc_reader_count(const std::string& topic)
{
    std::lock_guard<std::mutex> lk(reader_mutex_);
    reader_counts_[topic]++;
}

void StatsManager::set_writer_matched_count(const std::string& topic, uint32_t count)
{
    std::lock_guard<std::mutex> lk(writer_mutex_);
    writer_matched_[topic] = count;
}

void StatsManager::set_reader_matched_count(const std::string& topic, uint32_t count)
{
    std::lock_guard<std::mutex> lk(reader_mutex_);
    reader_matched_[topic] = count;
}

void StatsManager::set_output_format(const std::string& fmt)
{
    if (fmt == "json" || fmt == "JSON") format_ = OutputFormat::JSON;
    else if (fmt == "csv" || fmt == "CSV") format_ = OutputFormat::CSV;
    else format_ = OutputFormat::Text;
}

void StatsManager::set_entity_snapshot(size_t participants, size_t publishers, size_t subscribers, size_t writers, size_t readers, size_t topics)
{
    std::lock_guard<std::mutex> lk(entity_mutex_);
    participants_ = participants;
    publishers_ = publishers;
    subscribers_ = subscribers;
    writers_ = writers;
    readers_ = readers;
    topics_ = topics;
}

StatsSnapshot StatsManager::snapshot_and_reset_counts()
{
    StatsSnapshot s;
    // timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _MSC_VER
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    s.timestamp = oss.str();

    s.ipc_in = ipc_in_.exchange(0, std::memory_order_relaxed);
    s.ipc_out = ipc_out_.exchange(0, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lk(entity_mutex_);
        s.participants = participants_;
        s.publishers = publishers_;
        s.subscribers = subscribers_;
        s.writers = writers_;
        s.readers = readers_;
        s.topics = topics_;
    }

    {
        std::lock_guard<std::mutex> lk(writer_mutex_);
        s.writer_counts = std::move(writer_counts_);
        s.writer_matched = writer_matched_;
        writer_counts_.clear();
        writer_matched_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(reader_mutex_);
        s.reader_counts = std::move(reader_counts_);
        s.reader_matched = reader_matched_;
        reader_counts_.clear();
        reader_matched_.clear();
    }

    return s;
}

void StatsManager::scheduler_thread()
{
    while (running_) {
        // compute sleep until next minute boundary (00 seconds)
        using namespace std::chrono;
        auto now = system_clock::now();
        auto next = time_point_cast<minutes>(now) + minutes(1);
        std::this_thread::sleep_until(next);

        if (!running_) break;
        auto snap = snapshot_and_reset_counts();
        output_snapshot(snap);
    }
}

void StatsManager::output_snapshot(const StatsSnapshot& s)
{
    std::ostringstream out;
    out << "[STATS] " << s.timestamp << " IPC_IN=" << s.ipc_in << " IPC_OUT=" << s.ipc_out
        << " PARTICIPANTS=" << s.participants << " PUBLISHERS=" << s.publishers
        << " SUBSCRIBERS=" << s.subscribers << " WRITERS=" << s.writers
        << " READERS=" << s.readers << " TOPICS=" << s.topics << "\n";

    if (!s.writer_counts.empty()) {
        out << "  WriterCounts:\n";
        for (const auto& kv : s.writer_counts) {
            out << "    " << kv.first << " = " << kv.second << "\n";
        }
    }
    if (!s.reader_counts.empty()) {
        out << "  ReaderCounts:\n";
        for (const auto& kv : s.reader_counts) {
            out << "    " << kv.first << " = " << kv.second << "\n";
        }
    }

    // Console
    // Output according to selected format
    if (format_ == OutputFormat::Text) {
        std::cout << out.str();
        if (file_output_ && !file_path_.empty()) {
            std::ofstream f(file_path_, std::ios::app);
            if (f.is_open()) { f << out.str(); f.close(); }
        }
    } else if (format_ == OutputFormat::CSV) {
        // CSV: header lines then rows
        std::ostringstream csv;
        csv << "timestamp,metric,scope,key,value\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,participant_count," << s.participants << "\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,publisher_count," << s.publishers << "\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,subscriber_count," << s.subscribers << "\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,writers_total," << s.writers << "\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,readers_total," << s.readers << "\n";
        csv << s.timestamp << ",ENTITY_SNAPSHOT,domain=0,topics_total," << s.topics << "\n";
        csv << s.timestamp << ",IPC,,in," << s.ipc_in << "\n";
        csv << s.timestamp << ",IPC,,out," << s.ipc_out << "\n";
        for (const auto &kv : s.writer_counts) {
            uint32_t matched = 0;
            auto it = s.writer_matched.find(kv.first);
            if (it != s.writer_matched.end()) matched = it->second;
            csv << s.timestamp << ",MSG_COUNT," << kv.first << ",writer_writes," << kv.second << "\n";
            csv << s.timestamp << ",MSG_COUNT," << kv.first << ",writer_matched," << matched << "\n";
        }
        for (const auto &kv : s.reader_counts) {
            uint32_t matched = 0;
            auto it = s.reader_matched.find(kv.first);
            if (it != s.reader_matched.end()) matched = it->second;
            csv << s.timestamp << ",MSG_COUNT," << kv.first << ",reader_takes," << kv.second << "\n";
            csv << s.timestamp << ",MSG_COUNT," << kv.first << ",reader_matched," << matched << "\n";
        }
        std::cout << csv.str();
        if (file_output_ && !file_path_.empty()) { std::ofstream f(file_path_, std::ios::app); if (f.is_open()) { f << csv.str(); f.close(); } }
    } else { // JSON
        nlohmann::json j;
        j["timestamp"] = s.timestamp;
        j["ipc"] = { {"in", s.ipc_in}, {"out", s.ipc_out} };
        j["entities"] = {
            {"participants", s.participants},
            {"publishers", s.publishers},
            {"subscribers", s.subscribers},
            {"writers", s.writers},
            {"readers", s.readers},
            {"topics", s.topics}
        };
        nlohmann::json msgs = nlohmann::json::object();
        for (const auto &kv : s.writer_counts) {
            nlohmann::json tt;
            tt["writer_writes"] = kv.second;
            auto it = s.writer_matched.find(kv.first);
            tt["writer_matched"] = (it != s.writer_matched.end() ? it->second : 0);
            msgs[kv.first]["writer"] = tt;
        }
        for (const auto &kv : s.reader_counts) {
            nlohmann::json tt;
            tt["reader_takes"] = kv.second;
            auto it = s.reader_matched.find(kv.first);
            tt["reader_matched"] = (it != s.reader_matched.end() ? it->second : 0);
            msgs[kv.first]["reader"] = tt;
        }
        j["messages"] = msgs;
        std::string outj = j.dump();
        std::cout << outj << std::endl;
        if (file_output_ && !file_path_.empty()) { std::ofstream f(file_path_, std::ios::app); if (f.is_open()) { f << outj << std::endl; f.close(); } }
    }
}

} // namespace rtpdds
