#pragma once

#include<Core/StateFlags/AtomicStateFlags.h>
#include<Core/RefCount/AtomicRefCount.h>
#include<Core/Identifier/GUID.h>
#include<Core/Asset/AssetState.h>

#include<atomic>
#include<stdexcept>

namespace Pitaya::Core
{
    template<typename T>
    class AssetRef
    {
    public:
        struct AssetEntry final
        {
            std::atomic<T*> Data = nullptr;
            Pitaya::Core::GUID GUID;
            std::atomic<uint64_t> LoadGeneration = 0;
            Pitaya::Core::AtomicRefCount RefCount;
            Pitaya::Core::AtomicStateFlags<AssetState> State = AssetState::Empty;

            inline bool IsCpuReady() const noexcept
            {
                return State.HasBits(AssetState::CPULoaded) && !State.HasBits(AssetState::Unload);
            }
            inline bool IsGpuReady() const noexcept
            {
                return State.HasBits(AssetState::GPULoaded) && !State.HasBits(AssetState::Unload);
            }
            inline bool IsReady() const noexcept
            {
                return IsCpuReady() && IsGpuReady() && Data.load(std::memory_order_acquire) != nullptr;
            }
            inline bool HasLoadedData() const noexcept
            {
                return State.HasAllBits(AssetState::CPULoaded, AssetState::GPULoaded) &&
                    Data.load(std::memory_order_acquire) != nullptr;
            }
            inline bool IsLoading() const noexcept
            {
                return State.HasBits(AssetState::CPULoading) || State.HasBits(AssetState::GPULoading);
            }
            inline bool IsFailed() const noexcept
            {
                return State.HasBits(AssetState::CPUFailed) || State.HasBits(AssetState::GPUFailed);
            }
            inline bool IsUnloadRequested() const noexcept
            {
                return State.HasBits(AssetState::Unload);
            }
            inline void BeginCpuLoad() noexcept
            {
                State.ClearBits(AssetState::Unload);
                State.ClearBits(AssetState::CPULoaded);
                State.ClearBits(AssetState::CPUFailed);
                State.SetBits(AssetState::CPULoading);
            }
            inline void CompleteCpuLoad() noexcept
            {
                State.ModifyBits(AssetState::CPULoaded, AssetState::CPULoading);
                State.ClearBits(AssetState::CPUFailed);
            }
            inline void FailCpuLoad() noexcept
            {
                State.ModifyBits(AssetState::CPUFailed, AssetState::CPULoading);
            }
            inline void BeginGpuLoad() noexcept
            {
                State.ClearBits(AssetState::GPULoaded);
                State.ClearBits(AssetState::GPUFailed);
                State.SetBits(AssetState::GPULoading);
            }
            inline void CompleteGpuLoad() noexcept
            {
                State.ModifyBits(AssetState::GPULoaded, AssetState::GPULoading);
                State.ClearBits(AssetState::GPUFailed);
            }
            inline void FailGpuLoad() noexcept
            {
                State.ModifyBits(AssetState::GPUFailed, AssetState::GPULoading);
            }
            inline void RequestUnload() noexcept
            {
                State.SetBits(AssetState::Unload);
                LoadGeneration.fetch_add(1, std::memory_order_acq_rel);
            }
            inline void PrepareReload() noexcept
            {
                State.ClearBits(AssetState::Unload);
                State.ClearBits(AssetState::CPULoading);
                State.ClearBits(AssetState::CPULoaded);
                State.ClearBits(AssetState::CPUFailed);
                State.ClearBits(AssetState::GPULoading);
                State.ClearBits(AssetState::GPULoaded);
                State.ClearBits(AssetState::GPUFailed);
            }

            inline uint64_t BeginLoadGeneration() noexcept
            {
                return LoadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
            }

            inline bool IsLoadGeneration(uint64_t generation) const noexcept
            {
                return generation == 0 || LoadGeneration.load(std::memory_order_acquire) == generation;
            }
        };

    public:
        AssetRef() noexcept = default;
        ~AssetRef() noexcept
        {
            if (entry) { entry->RefCount.ReduceRefCount(); }
        }
        AssetRef(std::nullptr_t) noexcept
            : entry(nullptr) {}
        explicit AssetRef(AssetEntry* entry) noexcept
            : entry(entry)
        {
            if (entry) { entry->RefCount.AddRefCount(); }
        }
        AssetRef(const AssetRef& other) noexcept
            : entry(other.entry)
        {
            if (entry) { entry->RefCount.AddRefCount(); }
        }
        AssetRef& operator=(const AssetRef& other) noexcept
        {
            if (this == &other) { return *this; }
            if (entry) { entry->RefCount.ReduceRefCount(); }
            entry = other.entry;
            if (entry) { entry->RefCount.AddRefCount(); }
            return *this;
        }
        AssetRef(AssetRef&& other) noexcept
            : entry(other.entry)
        {
            other.entry = nullptr;
        }
        AssetRef& operator=(AssetRef&& other) noexcept
        {
            if (this == &other) { return *this; }
            if (entry) { entry->RefCount.ReduceRefCount(); }
            entry = other.entry;
            other.entry = nullptr;
            return *this;
        }
        AssetRef& operator=(std::nullptr_t) noexcept
        {
            if (entry) { entry->RefCount.ReduceRefCount(); }
            entry = nullptr;
            return *this;
        }
        AssetRef& operator=(AssetEntry* entry) noexcept
        {
            if (this->entry == entry) { return *this; }
            if (this->entry) { this->entry->RefCount.ReduceRefCount(); }
            this->entry = entry;
            if (this->entry) { this->entry->RefCount.AddRefCount(); }
            return *this;
        }

    public:
        explicit operator bool() const noexcept
        {
            return entry != nullptr;
        }
        bool operator==(std::nullptr_t) const noexcept
        {
            return entry == nullptr;
        }
        bool operator!=(std::nullptr_t) const noexcept
        {
            return entry != nullptr;
        }
        bool operator==(const AssetRef& other) const noexcept
        {
            return entry == other.entry;
        }
        bool operator!=(const AssetRef& other) const noexcept
        {
            return entry != other.entry;
        }

    public:
        T& operator*() const
        {
            if (!entry || !entry->IsCpuReady())
            {
                throw std::runtime_error("Dereferencing asset whose CPU data is not ready");
            }
            T* data = entry->Data.load(std::memory_order_acquire);
            if (!data)
            {
                throw std::runtime_error("Dereferencing asset with null data GUID:" + entry->GUID.ToString());
            }
            return *data;
        }
        T* operator->() const noexcept
        {
            return entry && entry->IsCpuReady() ? entry->Data.load(std::memory_order_acquire) : nullptr;
        }

    public:
        inline bool IsReady() const noexcept
        {
            return entry && entry->IsReady();
        }
        inline bool IsCpuReady() const noexcept
        {
            return entry && entry->IsCpuReady();
        }
        inline bool IsGpuReady() const noexcept
        {
            return entry && entry->IsGpuReady();
        }
        inline bool IsLoading() const noexcept
        {
            return entry && entry->IsLoading();
        }
        inline bool IsFailed() const noexcept
        {
            return entry && entry->IsFailed();
        }
        inline bool IsUnloadRequested() const noexcept
        {
            return entry && entry->IsUnloadRequested();
        }
        inline Pitaya::Core::AssetState GetState() const noexcept
        {
            return entry ? entry->State.GetEnum() : Pitaya::Core::AssetState::Empty;
        }
        inline uint32_t UseCount() const noexcept
        {
            return entry ? entry->RefCount.GetRefCount() : 0;
        }
        inline T* GetNativeAssetData() const noexcept
        {
            return entry ? entry->Data.load(std::memory_order_acquire) : nullptr;
        }
        inline Pitaya::Core::GUID GetGUID() const noexcept
        {
            return entry ? entry->GUID : Pitaya::Core::GUID();
        }

    private:
        mutable AssetEntry* entry = nullptr;
    };
}
