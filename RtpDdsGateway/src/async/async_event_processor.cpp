#include "async/async_event_processor.hpp"
#include "../../DkmRtpIpc/include/triad_thread.hpp"
// 현재는 헤더에서 전부 인라인. 유지보수 목적의 cpp 파일만 생성.
// TODO(next): 큐 용량, 드롭 정책, 메트릭 구현 시 이 cpp에 로직 이동.

#include <chrono>

namespace rtpdds { namespace async {

// 보조 생성자 추가 이유:
// 헤더에서 기본 인자를 Config()로 쓰는 대신, 컴파일러 간의 초기화/멤버 이니셜라이저
// 처리 차이를 회피하기 위해 여기서 기본 생성자를 추가하고 Config를 위임합니다.
// 이 변경은 Linux/GCC에서의 빌드 호환성을 높이기 위한 것이며, 기존 동작(구성 전달)은
// 그대로 유지됩니다.
AsyncEventProcessor::AsyncEventProcessor() : AsyncEventProcessor(Config()) {}

AsyncEventProcessor::AsyncEventProcessor(const Config& cfg) : cfg_(cfg) {}

AsyncEventProcessor::~AsyncEventProcessor()
{
	stop();
}

void AsyncEventProcessor::start()
{
	bool expected = false;
	if (!running_.compare_exchange_strong(expected, true)) return;
	// VxWorks에서는 TriadThread::start()로 1MB 스택을 적용, 기타 플랫폼은 std::thread 생성
#ifdef RTI_VXWORKS
	worker_.start([this] { loop(); }, "DA_AsyncWkr");
	if (cfg_.monitor_sec > 0) monitor_.start([this] { monitor_loop(); }, "DA_AsyncMon");
#else
	worker_ = std::thread([this] { 
		triad::set_thread_name("DA_AsyncWkr"); 
		loop(); 
	});
	if (cfg_.monitor_sec > 0) monitor_ = std::thread([this] { 
		triad::set_thread_name("DA_AsyncMon"); 
		monitor_loop(); 
	});
#endif
	LOG_INF("ASYNC", "start max_q=%zu monitor=%ds drain=%d warn_us=%u", cfg_.max_queue, cfg_.monitor_sec,
			cfg_.drain_stop, cfg_.exec_warn_us);
}

void AsyncEventProcessor::stop()
{
	bool expected = true;
	if (!running_.compare_exchange_strong(expected, false)) return;
	cv_.notify_all();
	if (worker_.joinable()) worker_.join();
	if (monitor_.joinable()) monitor_.join();
}

bool AsyncEventProcessor::is_running() const
{
	return running_.load();
}

void AsyncEventProcessor::enqueue(std::function<void()> fn)
{
	{
		std::lock_guard<std::mutex> lk(m_);
		if (q_.size() >= cfg_.max_queue) {
			stats_drop_.fetch_add(1);
			if (error_handler_) error_handler_("queue overflow", "AsyncEventProcessor::enqueue");
			LOG_WRN("ASYNC", "drop queue_full depth=%zu", q_.size());
			return;
		}
		q_.push_back(std::move(fn));
		if (q_.size() > max_depth_) max_depth_ = q_.size();
	}
	cv_.notify_one();
}

void AsyncEventProcessor::loop()
{
	for (;;) {
		std::function<void()> job;
		{
			std::unique_lock<std::mutex> lk(m_);
			cv_.wait(lk, [this] { return !running_.load() || !q_.empty(); });
			if (!running_.load() && q_.empty()) break;

			if (!running_.load() && !cfg_.drain_stop && !q_.empty()) {
				stats_drop_.fetch_add(q_.size());
				q_.clear();
				break;
			}
			if (!q_.empty()) {
				job = std::move(q_.front());
				q_.pop_front();
			}
		}

		const auto t0 = std::chrono::steady_clock::now();
		try {
			if (job) job();
		} catch (const std::exception& e) {
			if (error_handler_) error_handler_(e.what(), "AsyncEventProcessor::loop");
			LOG_ERR("ASYNC", "exec exception=%s", e.what());
		}
		const auto usec =
			(uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0)
				.count();
		if (usec > cfg_.exec_warn_us) {
			LOG_WRN("ASYNC", "slow job exec_us=%llu", (unsigned long long)usec);
		}
		stats_exec_.fetch_add(1);
	}
}

void AsyncEventProcessor::monitor_loop()
{
	while (running_.load()) {
		std::this_thread::sleep_for(std::chrono::seconds(cfg_.monitor_sec));
		auto st = get_stats();
		LOG_INF("ASYNC", "stats enq(s/c/e)=(%llu/%llu/%llu) exec=%llu drop=%llu max_depth=%zu cur_depth=%zu",
				(unsigned long long)st.enq_sample, (unsigned long long)st.enq_cmd, (unsigned long long)st.enq_err,
				(unsigned long long)st.exec_jobs, (unsigned long long)st.dropped, st.max_depth, st.cur_depth);
	}
}

}} // namespace rtpdds::async

