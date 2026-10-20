#pragma once

#include<Core/Allocate/Allocate.h>
#include<Core/Asset/AssetRef.h>
#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Identifier/GUID.h>

#include<Asset/Common/AssetType.h>
#include<Asset/Common/Material.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/RenderTarget.h>
#include<Asset/Common/SkyBox.h>
#include<Asset/Common/Shader.h>
#include<Asset/Common/Texture.h>

#include<cstdint>
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
