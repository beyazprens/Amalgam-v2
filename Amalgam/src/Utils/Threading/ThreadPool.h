#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// ThreadPool - a fixed-size pool of worker threads that execute queued tasks.
// Designed for parallel workloads such as aimbot target scanning.
//
// Usage:
//   ThreadPool pool(4);
//   pool.enqueue([]() { /* work */ });
//   pool.wait_idle();
class ThreadPool
{
private:
	std::vector<std::thread> m_workers;
	std::queue<std::function<void()>> m_tasks;
	std::mutex m_queue_mutex;
	std::condition_variable m_condition;
	std::condition_variable m_idle_condition;
	std::atomic<size_t> m_active_tasks{ 0 };
	std::atomic<bool> m_stop{ false };

public:
	explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
	{
		if (num_threads == 0)
			num_threads = 1;

		m_workers.reserve(num_threads);
		for (size_t i = 0; i < num_threads; ++i)
		{
			m_workers.emplace_back([this]()
			{
				for (;;)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(m_queue_mutex);
						m_condition.wait(lock, [this]()
						{
							return m_stop.load() || !m_tasks.empty();
						});

						if (m_stop.load() && m_tasks.empty())
							return;

						task = std::move(m_tasks.front());
						m_tasks.pop();
						++m_active_tasks;  // increment while still holding the lock
					}

					task();

					--m_active_tasks;  // atomic; no lock needed
					m_idle_condition.notify_all();
				}
			});
		}
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_stop.store(true);
		}
		m_condition.notify_all();
		for (auto& worker : m_workers)
		{
			if (worker.joinable())
				worker.join();
		}
	}

	// Non-copyable
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	// Enqueue a task for execution by the next available worker thread.
	template<class F>
	void enqueue(F&& f)
	{
		{
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_tasks.emplace(std::forward<F>(f));
		}
		m_condition.notify_one();
	}

	// Block the calling thread until all queued tasks have completed.
	void wait_idle()
	{
		std::unique_lock<std::mutex> lock(m_queue_mutex);
		m_idle_condition.wait(lock, [this]()
		{
			return m_tasks.empty() && m_active_tasks.load() == 0;
		});
	}

	// Returns the number of worker threads in the pool.
	size_t size() const { return m_workers.size(); }
};
