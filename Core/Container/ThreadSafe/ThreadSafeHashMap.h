#pragma once

#include<unordered_map>
#include<shared_mutex> 
#include<mutex>        
#include<utility>      
#include<type_traits>  
#include<initializer_list>
#include<optional>
#include<vector>

namespace Pitaya::Core
{
    template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
    class ThreadSafeHashMap
    {
    public:
        ThreadSafeHashMap() noexcept = default;
        ~ThreadSafeHashMap() noexcept = default;

        ThreadSafeHashMap(std::initializer_list<typename std::unordered_map<Key, Value, Hash, KeyEqual>::value_type> init)
            : map(init) {}

    public:
        ThreadSafeHashMap(const ThreadSafeHashMap&) = delete;
        ThreadSafeHashMap& operator=(const ThreadSafeHashMap&) = delete;
        ThreadSafeHashMap(ThreadSafeHashMap&&) = delete;
        ThreadSafeHashMap& operator=(ThreadSafeHashMap&&) = delete;

    public:
        inline bool Contains(const Key& key) const
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            return map.find(key) != map.end();
        }
        inline size_t Size() const noexcept
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            return map.size();
        }
        inline bool Empty() const noexcept
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            return map.size() == 0;
        }

        inline std::unordered_map<Key, Value, Hash, KeyEqual> ExtractAll()
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            std::unordered_map<Key, Value, Hash, KeyEqual> out;
            std::swap(out, map);
            return out;
        }
        inline std::vector<std::pair<Key, Value>> GetAllKeyValuePairs() const
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            std::vector<std::pair<Key, Value>> result;
            result.reserve(map.size());
            for (const auto& [key, value] : map)
            {
                result.emplace_back(key, value);
            }
            return result;
        }
        inline bool Emplace(const Key& key, Value value)
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            auto [it, success] = map.emplace(key, std::move(value));
            return success;
        }
        inline void InsertOrAssign(const Key& key, Value value) noexcept
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            map.insert_or_assign(key, std::move(value));
        }
        inline size_t Erase(const Key& key)
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            return map.erase(key);
        }
        inline void Clear() noexcept
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            map.clear();
        }
        template <typename SuccessCallback, typename FailCallback>
        void FindAndReadKV(const Key& key, SuccessCallback&& successCallback, FailCallback&& failCallback) const
        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            auto it = map.find(key);
            if (it != map.end())
            {
                std::forward<SuccessCallback>(successCallback)(it->first, static_cast<const Value&>(it->second));
            }
            else
            {
                std::forward<FailCallback>(failCallback)(key);
            }
        }
        template <typename SuccessCallback, typename FailCallback>
        inline void FindOperateKV(const Key& key, SuccessCallback&& successCallback, FailCallback&& failCallback)
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            auto it = map.find(key);
            if (it != map.end())
            {
                std::forward<SuccessCallback>(successCallback)(it->first, it->second);
            }
            else
            {
                std::forward<FailCallback>(failCallback)(key);
            }
        }
        template <typename SuccessCallback, typename InsertDecisionCallback>
        inline void FindOperateKVOrInsert(const Key& key, SuccessCallback&& successCallback, InsertDecisionCallback&& insertDecisionCallback)
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            auto it = map.find(key);
            if (it != map.end())
            {
                std::forward<SuccessCallback>(successCallback)(it->first, it->second);
            }
            else
            {
                std::optional<std::pair<Key, Value>> insertPair =
                    std::forward<InsertDecisionCallback>(insertDecisionCallback)(key);
                if (insertPair.has_value())
                {
                    map.emplace(std::move(insertPair.value()));
                }
            }
        }
        template <typename CheckFunction>
        inline void ForEachCheckErase(CheckFunction&& checkFunction)
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            auto it = map.begin();
            while (it != map.end())
            {
                if (std::forward<CheckFunction>(checkFunction)(it->first, it->second))
                {
                    it = map.erase(it);
                    continue;
                }

                ++it;
            }
        }

    private:
        std::unordered_map<Key, Value, Hash, KeyEqual> map;
        mutable std::shared_mutex mutex;           
    };
}
