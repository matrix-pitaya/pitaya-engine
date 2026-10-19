#pragma once

#include<Core/Thread/Thread.h>
#include<Core/Allocate/Allocate.h>
#include<Core/PassKey/PassKey.h>
#include<Context/Common/Module.h>
#include<Log/Common/FuncTable.h>

#include<queue>
#include<mutex>
#include<functional>

namespace Pitaya::Engine { class Engine; }

namespace Pitaya::Task
{
	class TaskScheduler
	{
	public:
		struct Factory
		{
			friend class Pitaya::Engine::Module<TaskScheduler>;
		private:
			static TaskScheduler* Create()
			{
				return PITAYA_NEW(TaskScheduler);
			}
			static void Destroy(TaskScheduler* taskScheduler)
			{
				PITAYA_DELETE(taskScheduler);
			}
		};
		struct Lifecycle
		{
			friend class Pitaya::Engine::Module<TaskScheduler>;
		private:
			static bool Initialize(TaskScheduler* taskScheduler)
			{
				return taskScheduler->Initialize();
			}
			static void Release(TaskScheduler* taskScheduler)
			{
				taskScheduler->Release();
			}
		};

	private:
		struct Job
		{
			explicit Job(std::function<void()> func, std::string_view name)
				:func(std::move(func)), name(name) {}
			Job() = default;
			Job(const Job&) = delete;
			Job& operator=(const Job&) = delete;
			Job(Job&&) = default;
			Job& operator=(Job&&) = default;
			void operator()()
			{
				if (!func)
				{
					Pitaya::Log::Warning("{} Job Is Empty!", name);
					return;
				}

				Pitaya::Log::Info("{} Job Star!", name);
				func();
				Pitaya::Log::Info("{} Job Over!", name);
			}

		private:
			std::function<void()> func;
			std::string name;
		};

	private:
		TaskScheduler() = default;
		~TaskScheduler() = default;

	public:
		TaskScheduler(const TaskScheduler&) = delete;
		TaskScheduler& operator=(const TaskScheduler&) = delete;
		TaskScheduler(TaskScheduler&&) = delete;
		TaskScheduler& operator=(TaskScheduler&&) = delete;

	private:
		bool Initialize();
		void Release();

	public:
		void TuneThreadPool(Pitaya::Core::PassKey<Pitaya::Engine::Engine>);

	public:
		inline void PostJob(std::function<void()> func, std::string_view name = "Unknown")
		{
			Pitaya::Log::Info("{} Job is Posted!", name);
			{
				std::lock_guard<std::mutex> lock(mutex);
                if (!isRunning.load(std::memory_order_acquire))
                {
                    Pitaya::Log::Warning("{} Job is rejected because task scheduler is stopping", name);
                    return;
                }
				jobs.emplace(std::move(func), name);
			}
			cond.notify_one();
		}

	private:
		inline static void BootstrapJobThread(void* task, void*)
		{
			static_cast<Pitaya::Task::TaskScheduler*>(task)->JobThread();
		}
		inline void JobThread()
		{
			while (true)
			{
				Job job;
				{
					std::unique_lock<std::mutex> lock(mutex);
					cond.wait(lock, [this] {return !jobs.empty() || !isRunning.load(std::memory_order_acquire); });
					if (!isRunning.load(std::memory_order_acquire) && jobs.empty()) { break; }
					job = std::move(jobs.front());
					jobs.pop();
				}
				job();
			}
		}

	private:
		std::queue<Job> jobs;
		mutable std::mutex mutex;
		std::condition_variable cond;
		std::vector<Pitaya::Core::Thread::Identifier> jobThreads;
		std::atomic<bool> isRunning = false;
	};
}
