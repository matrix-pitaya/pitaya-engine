#pragma once

#include<queue>
#include<mutex>
#include<utility>

namespace Pitaya::Core
{
	template <typename T>
	class ThreadSafeQueue
	{
	public:
		ThreadSafeQueue() = default;
		~ThreadSafeQueue() = default;
		ThreadSafeQueue(const ThreadSafeQueue&) = delete;
		ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
		ThreadSafeQueue(ThreadSafeQueue&&) = delete;
		ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

	public:
        inline bool Empty() const
        {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.empty();
        }
        inline void Push(const T& item)
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(item);
        }
		inline void Push(T&& item)
		{
			std::lock_guard<std::mutex> lock(mutex);
			queue.push(std::move(item));
		}
		template<typename... Args>
		inline void Emplace(Args&&... args)
		{
			std::lock_guard<std::mutex> lock(mutex);
			queue.emplace(std::forward<Args>(args)...);
		}
        inline bool Pop(T& outItem)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty())
            {
                return false;
            }

            outItem = std::move(queue.front());
            queue.pop();
            return true;
        }
        inline std::queue<T> PopAll()
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::queue<T> out;
            std::swap(out, queue);
            return out;
        }
        inline std::queue<T> PopN(size_t n)
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::queue<T> out;
            for (size_t i = 0; i < n && !queue.empty(); ++i)
            {
                out.push(std::move(queue.front()));
                queue.pop();
            }
            return out;
        }

	private:
		mutable std::mutex mutex;
		std::queue<T> queue;
	};
}
