#pragma once

#include<Core/Allocate/Allocate.h>
#include<Core/Asset/AssetRef.h>
#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Identifier/GUID.h>

#include<Asset/Common/AssetType.h>
#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/Material.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/RenderTarget.h>
#include<Asset/Common/SkyBox.h>
#include<Asset/Common/Shader.h>
#include<Asset/Common/Texture.h>

#include<cstdint>
#include<algorithm>
#include<optional>

namespace Pitaya::Asset
{
    // RuntimeAssetManager 只拥有运行时 Entry 索引。
    // 加载策略和旧 API 仍由 AssetHub facade 驱动，后续阶段再迁移到这里。
    class RuntimeAssetManager final
    {
        friend class AssetHub;

    private:
        RuntimeAssetManager() = default;
        ~RuntimeAssetManager() = default;

    public:
        RuntimeAssetManager(const RuntimeAssetManager&) = delete;
        RuntimeAssetManager& operator=(const RuntimeAssetManager&) = delete;
        RuntimeAssetManager(RuntimeAssetManager&&) = delete;
        RuntimeAssetManager& operator=(RuntimeAssetManager&&) = delete;

        template<AssetType T>
        struct AcquireResult
        {
            Pitaya::Core::AssetRef<T> Asset = nullptr;
            uint64_t Generation = 0;
            bool ShouldLoad = false;
        };

        template<AssetType T>
        inline AcquireResult<T> Acquire(Pitaya::Core::GUID guid)
        {
            AcquireResult<T> result;
            GetAssetEntryMap<T>().FindOperateKVOrInsert(guid,
                [&result](Pitaya::Core::GUID, typename Pitaya::Core::AssetRef<T>::AssetEntry* entry)
                {
                    if (!entry) { return; }
                    result.Asset = entry;
                    if (entry->IsUnloadRequested() || entry->IsFailed())
                    {
                        entry->PrepareReload();
                        result.Generation = entry->BeginLoadGeneration();
                        result.ShouldLoad = true;
                    }
                },
                [&result](Pitaya::Core::GUID _guid)
                    -> std::optional<std::pair<Pitaya::Core::GUID,
                        typename Pitaya::Core::AssetRef<T>::AssetEntry*>>
                {
                    auto* entry = Pitaya::Core::New<typename Pitaya::Core::AssetRef<T>::AssetEntry>();
                    if (!entry) { return std::nullopt; }
                    entry->GUID = _guid;
                    entry->Data.store(Pitaya::Core::New<T>(), std::memory_order_release);
                    entry->SetOrphanReclaimFunction(&DestroyEntry<T>);
                    entry->BeginCpuLoad();
                    result.Asset = entry;
                    result.Generation = entry->BeginLoadGeneration();
                    result.ShouldLoad = true;
                    return std::make_pair(_guid, entry);
                });
            return result;
        }

        struct UnloadResult
        {
            bool Success = false;
            bool HasGpuOperation = false;
            Pitaya::Asset::AssetOperate Operation;
        };

        template<AssetType T>
        inline UnloadResult RequestUnload(Pitaya::Core::GUID guid)
        {
            UnloadResult result;
            GetAssetEntryMap<T>().FindOperateKV(guid,
                [this, &result](Pitaya::Core::GUID, typename Pitaya::Core::AssetRef<T>::AssetEntry* entry)
                {
                    if (!entry) { return; }
                    if (entry->IsUnloadRequested())
                    {
                        result.Success = true;
                        return;
                    }
                    if (entry->RefCount.HasRef() || entry->IsLoading()) { return; }

                    entry->RequestUnload();
                    result.HasGpuOperation = BuildGpuUnload<T>(entry, result.Operation);
                    ReleaseCpuDependencies<T>(entry);
                    result.Success = true;
                },
                [](Pitaya::Core::GUID) {});
            return result;
        }

        inline void CollectUnusedAssets(size_t budget)
        {
            size_t remaining = budget;
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::Texture>(remaining));
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::Shader>(remaining));
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::Mesh>(remaining));
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::Material>(remaining));
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::RenderTarget>(remaining));
            remaining -= std::min(remaining, CollectUnusedEntries<Pitaya::Asset::SkyBox>(remaining));
        }

        template<AssetType T>
        inline auto& GetAssetEntryMap() noexcept
        {
            if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>) { return textures; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>) { return shaders; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>) { return meshes; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Material>) { return materials; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::RenderTarget>) { return rendertargets; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>) { return skyboxes; }
        }

        inline auto& Textures() noexcept { return textures; }
        inline auto& Shaders() noexcept { return shaders; }
        inline auto& Meshes() noexcept { return meshes; }
        inline auto& Materials() noexcept { return materials; }
        inline auto& RenderTargets() noexcept { return rendertargets; }
        inline auto& SkyBoxes() noexcept { return skyboxes; }

    private:
        template<AssetType T>
        inline size_t CollectUnusedEntries(size_t budget)
        {
            if (budget == 0) { return 0; }
            size_t collected = 0;
            GetAssetEntryMap<T>().ForEachCheckErase(
                [&collected, budget](Pitaya::Core::GUID,
                    typename Pitaya::Core::AssetRef<T>::AssetEntry*& entry)
                {
                    if (collected >= budget) { return false; }
                    if (!entry || !entry->IsUnloadRequested() || entry->RefCount.HasRef() || entry->IsLoading())
                    {
                        return false;
                    }

                    {
                        std::lock_guard<std::mutex> lifecycleLock(entry->LifecycleMutex);
                        auto* data = entry->Data.exchange(nullptr, std::memory_order_acq_rel);
                        Pitaya::Core::Delete(data);
                        entry->State = Pitaya::Core::AssetState::Empty;
                    }
                    entry->MarkOrphaned();
                    entry = nullptr;
                    ++collected;
                    return true;
                });
            return collected;
        }

        template<AssetType T>
        inline bool BuildGpuUnload(typename Pitaya::Core::AssetRef<T>::AssetEntry* entry,
            Pitaya::Asset::AssetOperate& operation)
        {
            if (!entry) { return false; }
            std::lock_guard<std::mutex> lifecycleLock(entry->LifecycleMutex);
            T* data = entry->Data.load(std::memory_order_acquire);
            if (!data) { return false; }

            bool hasGpuHandle = false;
            if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>)
            {
                if (data->Type == Pitaya::GPU::TextureType::Texture2D && data->Texture2DHandle)
                {
                    operation.Data = Pitaya::Asset::Texture2DUnloadRequire{ data->Texture2DHandle };
                    data->Texture2DHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle::Invalid;
                    hasGpuHandle = true;
                }
                else if (data->Type == Pitaya::GPU::TextureType::Texture2DArray && data->Texture2DArrayHandle)
                {
                    operation.Data = Pitaya::Asset::Texture2DArrayUnloadRequire{ data->Texture2DArrayHandle };
                    data->Texture2DArrayHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Texture2DArray>::Handle::Invalid;
                    hasGpuHandle = true;
                }
                else if (data->Type == Pitaya::GPU::TextureType::TextureCubemap && data->TextureCubemapHandle)
                {
                    operation.Data = Pitaya::Asset::TextureCubemapUnloadRequire{ data->TextureCubemapHandle };
                    data->TextureCubemapHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
                    hasGpuHandle = true;
                }
            }
            else if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>)
            {
                if (data->ShaderHandle)
                {
                    operation.Data = Pitaya::Asset::ShaderUnloadRequire{ data->ShaderHandle };
                    data->ShaderHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Shader>::Handle::Invalid;
                    hasGpuHandle = true;
                }
            }
            else if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>)
            {
                if (data->VertexArrayHandle || data->VertexBufferHandle || data->IndexBufferHandle)
                {
                    operation.Data = Pitaya::Asset::MeshUnloadRequire{
                        data->VertexArrayHandle, data->VertexBufferHandle, data->IndexBufferHandle };
                    data->VertexArrayHandle = Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle::Invalid;
                    data->VertexBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle::Invalid;
                    data->IndexBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle::Invalid;
                    hasGpuHandle = true;
                }
            }
            else if constexpr (std::is_same_v<T, Pitaya::Asset::RenderTarget>)
            {
                if (data->SceneFrameBufferHandle || data->PingPongFrameBufferHandles[0]
                    || data->PingPongFrameBufferHandles[1] || data->FinalFrameBufferHandle)
                {
                    operation.Data = Pitaya::Asset::RenderTargetUnloadRequire{
                        data->SceneFrameBufferHandle,
                        { data->PingPongFrameBufferHandles[0], data->PingPongFrameBufferHandles[1] },
                        data->FinalFrameBufferHandle };
                    data->SceneFrameBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
                    data->PingPongFrameBufferHandles[0] = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
                    data->PingPongFrameBufferHandles[1] = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
                    data->FinalFrameBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
                    hasGpuHandle = true;
                }
            }
            else if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>)
            {
                if (data->EquirectHandle || data->EnvCubemapHandle || data->IrradianceHandle || data->PrefilteredHandle)
                {
                    operation.Data = Pitaya::Asset::SkyBoxUnloadRequire{
                        data->EquirectHandle, data->EnvCubemapHandle,
                        data->IrradianceHandle, data->PrefilteredHandle };
                    data->EquirectHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle::Invalid;
                    data->EnvCubemapHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
                    data->IrradianceHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
                    data->PrefilteredHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
                    hasGpuHandle = true;
                }
            }
            return hasGpuHandle;
        }

        template<AssetType T>
        inline void ReleaseCpuDependencies(typename Pitaya::Core::AssetRef<T>::AssetEntry* entry)
        {
            if (!entry) { return; }
            std::lock_guard<std::mutex> lifecycleLock(entry->LifecycleMutex);
            T* data = entry->Data.load(std::memory_order_acquire);
            if (!data) { return; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>)
            {
                data->Materials.clear();
                data->SubMeshs.clear();
                data->BoneInverseMatrices.clear();
                data->BoneCount = 0;
            }
            else if constexpr (std::is_same_v<T, Pitaya::Asset::Material>)
            {
                data->Shader = nullptr;
                data->Textures.clear();
                data->FloatParams.clear();
                data->VectorParams.clear();
            }
        }

        template<AssetType AssetClass>
        inline static void DestroyEntry(typename Pitaya::Core::AssetRef<AssetClass>::AssetEntry* entry) noexcept
        {
            PITAYA_DELETE(entry);
        }

        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry*> meshes;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry*> textures;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry*> shaders;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry*> materials;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry*> rendertargets;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry*> skyboxes;
    };
}
