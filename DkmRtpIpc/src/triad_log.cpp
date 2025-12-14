#include "triad_log.hpp"
#include "triad_thread.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>

#ifdef RTI_VXWORKS
#include <taskLib.h>
#endif

namespace fs = std::filesystem;

namespace triad {

    static Lvl g_current_level = Lvl::Info;

    void set_level(Lvl l) {
        g_current_level = l;
    }

    struct LogEntry {
        Lvl level;
        std::string timestamp;
        std::string thread_id;
        std::string tag;
        std::string file;
        int line;
        std::string message;
    };

    class AsyncLogger {
    public:
        static AsyncLogger& instance() {
            static AsyncLogger inst;
            return inst;
        }

        void start(const std::string& dir, const std::string& file, int max_mb, int backups, bool file_out, bool console) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (running_) return;

            log_dir_ = dir;
            base_filename_ = file;
            max_file_size_ = static_cast<uintmax_t>(max_mb) * 1024 * 1024;
            max_backup_files_ = backups;
            file_output_ = file_out;
            console_output_ = console;
            running_ = true;

            // 디렉토리 생성
            try {
                if (!fs::exists(log_dir_)) {
                    fs::create_directories(log_dir_);
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to create log directory: " << e.what() << std::endl;
            }

#ifdef RTI_VXWORKS
            worker_thread_.start([this]{ process_queue(); }, "DA_Logger");
#else
            worker_thread_ = std::thread([this]{ 
                triad::set_thread_name("DA_Logger"); 
                process_queue(); 
            });
#endif
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                running_ = false;
            }
            cv_.notify_one();
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
        }

        void push(LogEntry&& entry) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                log_queue_.push(std::move(entry));
            }
            cv_.notify_one();
        }

        bool is_running() const {
            std::lock_guard<std::mutex> lock(const_cast<AsyncLogger*>(this)->queue_mutex_);
            return running_;
        }

    private:
        void process_queue() {
            while (true) {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait(lock, [this]{ return !log_queue_.empty() || !running_; });

                if (!running_ && log_queue_.empty()) break;

                // 큐의 모든 내용을 처리 (Batch processing)
                while (!log_queue_.empty()) {
                    auto entry = std::move(log_queue_.front());
                    log_queue_.pop();
                    lock.unlock(); // 파일 쓰기 중에는 락 해제

                    write_log(entry);

                    lock.lock();
                }
            }
        }

        void write_log(const LogEntry& entry) {
            std::string log_line = format_log(entry, false);  // 파일용 (색상 없음)

            if (console_output_) {
                std::string console_line = format_log(entry, true);  // 콘솔용 (색상 있음)
                std::cout << console_line << std::flush; // 즉시 출력
            }

            if (!file_output_) return;

            try {
                fs::path log_path = fs::path(log_dir_) / base_filename_;
                
                // 로테이션 체크
                if (fs::exists(log_path) && fs::file_size(log_path) >= max_file_size_) {
                    rotate_logs(log_path);
                }

                std::ofstream ofs(log_path, std::ios::app);
                if (ofs.is_open()) {
                    ofs << log_line;
                }
            } catch (const std::exception& e) {
                if (console_output_) std::cerr << "Log write error: " << e.what() << std::endl;
            }
        }

        void rotate_logs(const fs::path& log_path) {
            // agent.log -> agent.1.log
            // agent.1.log -> agent.2.log ...
            
            fs::path last_backup = fs::path(log_dir_) / (base_filename_ + "." + std::to_string(max_backup_files_));
            if (fs::exists(last_backup)) {
                fs::remove(last_backup);
            }

            for (int i = max_backup_files_ - 1; i >= 1; --i) {
                fs::path src = fs::path(log_dir_) / (base_filename_ + "." + std::to_string(i));
                fs::path dst = fs::path(log_dir_) / (base_filename_ + "." + std::to_string(i + 1));
                if (fs::exists(src)) {
                    fs::rename(src, dst);
                }
            }

            fs::path first_backup = fs::path(log_dir_) / (base_filename_ + ".1");
            fs::rename(log_path, first_backup);
        }

        std::string format_log(const LogEntry& entry, bool use_color) {
            std::ostringstream oss;
            // Color codes for console
            const char* color = "";
            const char* reset = "\033[0m";
            const char* lvl_str = "INF";

            if (use_color) {
                switch (entry.level) {
                    case Lvl::Debug: color = "\033[90m"; lvl_str = "DBG"; break;  // 회색
                    case Lvl::Info:  color = "\033[37m"; lvl_str = "INF"; break;  // 흰색
                    case Lvl::Trace: color = "\033[36m"; lvl_str = "TRC"; break;  // 청록색
                    case Lvl::Warn:  color = "\033[33m"; lvl_str = "WRN"; break;  // 노란색
                    case Lvl::Error: color = "\033[31m"; lvl_str = "ERR"; break;  // 빨간색
                }
            } else {
                switch (entry.level) {
                    case Lvl::Debug: lvl_str = "DBG"; break;
                    case Lvl::Info:  lvl_str = "INF"; break;
                    case Lvl::Trace: lvl_str = "TRC"; break;
                    case Lvl::Warn:  lvl_str = "WRN"; break;
                    case Lvl::Error: lvl_str = "ERR"; break;
                }
            }

            // 파일명만 추출
            fs::path p(entry.file);
            std::string filename = p.filename().string();

            if (use_color) {
                oss << color << "[" << entry.timestamp << "] [" << lvl_str << "] [tid:" << entry.thread_id << "] "
                    << "[" << entry.tag << "] [" << filename << ":" << entry.line << "] "
                    << entry.message << reset << "\n";
            } else {
                oss << "[" << entry.timestamp << "] [" << lvl_str << "] [tid:" << entry.thread_id << "] "
                    << "[" << entry.tag << "] [" << filename << ":" << entry.line << "] "
                    << entry.message << "\n";
            }
            
            return oss.str();
        }

        // 멤버 변수
        std::string log_dir_;
        std::string base_filename_;
        uintmax_t max_file_size_;
        int max_backup_files_;
        bool file_output_ = true;
        bool console_output_ = true;
        
        std::queue<LogEntry> log_queue_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
        triad::TriadThread worker_thread_;
        bool running_ = false;
    };

    void init_logger(const std::string& log_dir, const std::string& filename, 
                     int max_size_mb, int max_files, bool file_out, bool console_out) {
        AsyncLogger::instance().start(log_dir, filename, max_size_mb, max_files, file_out, console_out);
    }

    void shutdown_logger() {
        AsyncLogger::instance().stop();
    }

    void logf(Lvl lvl, const char *tag, const char *file, int line, const char *fmt, ...) {
        if (lvl < g_current_level) return;

        // Format message
        va_list ap;
        va_start(ap, fmt);
        std::vector<char> buf(16 * 1024); // 16KB buffer
        std::vsnprintf(buf.data(), buf.size(), fmt, ap);
        va_end(ap);

        // Timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

        // Thread ID
        std::stringstream ss_tid;
#ifdef RTI_VXWORKS
        ss_tid << "0x" << std::hex << (unsigned long)taskIdSelf();
#else
        ss_tid << std::this_thread::get_id();
#endif

        // 로거가 실행 중이 아니면(설정 파일 없음 등) 콘솔에 직접 출력 (Fallback)
        if (!AsyncLogger::instance().is_running()) {
            const char* color = "";
            const char* reset = "\033[0m";
            const char* lvl_str = "INF";

            switch (lvl) {
                case Lvl::Debug: color = "\033[90m"; lvl_str = "DBG"; break;
                case Lvl::Info:  color = "\033[0m";  lvl_str = "INF"; break;
                case Lvl::Trace: color = "\033[36m"; lvl_str = "TRC"; break;
                case Lvl::Warn:  color = "\033[33m"; lvl_str = "WRN"; break;
                case Lvl::Error: color = "\033[31m"; lvl_str = "ERR"; break;
            }
            
            // 파일명만 추출
            fs::path p(file ? file : "-");
            std::string filename = p.filename().string();

            std::fprintf(stderr, "%s[%s] [%s] [tid:%s] [%s] [%s:%d] %s%s\n", 
                color, ts, lvl_str, ss_tid.str().c_str(), 
                tag ? tag : "-", filename.c_str(), line, buf.data(), reset);
            return;
        }

        LogEntry entry;
        entry.level = lvl;
        entry.timestamp = ts;
        entry.thread_id = ss_tid.str();
        entry.tag = tag ? tag : "-";
        entry.file = file ? file : "-";
        entry.line = line;
        entry.message = std::string(buf.data());

        AsyncLogger::instance().push(std::move(entry));
    }

} // namespace triad
