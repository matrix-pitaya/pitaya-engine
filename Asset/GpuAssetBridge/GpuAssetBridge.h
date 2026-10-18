#pragma once

namespace Pitaya::Render
{
    class Renderer;
}

#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/Texture.h>
#include<Asset/Common/TextureUsage.h>

#include<Core/Container/ThreadSafe/ThreadSafeQueue.h>
#include<Core/Asset/AssetRef.h>
#include<Core/PassKey/PassKey.h>
#include<Core/Utils/Check.h>
#include<Core/Utils/Time.h>

#include<Thread/Common/FuncTable.h>
#include<Thread/Common/ThreadType.h>

#include<GPU/Common/FuncTable.h>
#include<Render/Common/FuncTable.h>
#include<Log/Common/FuncTable.h>

#include<chrono>
#include<queue>
#include<type_traits>
#include<utility>
#include<variant>

namespace Pitaya::Asset
{
    class AssetHub;

    class GpuAssetBridge final
    {
        friend class AssetHub;

    private:
        GpuAssetBridge() = default;
        ~GpuAssetBridge() = default;

    public:
        GpuAssetBridge(const GpuAssetBridge&) = delete;
        GpuAssetBridge& operator=(const GpuAssetBridge&) = delete;
        GpuAssetBridge(GpuAssetBridge&&) = delete;
        GpuAssetBridge& operator=(GpuAssetBridge&&) = delete;

    private:
        using TextureEntry = Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry;
        using TextureEntryLookup = TextureEntry* (*)(void*, Pitaya::Core::GUID);

        inline void Enqueue(const Pitaya::Asset::AssetOperate& operate)
        {
            pendingOperations.Push(operate);
            Pitaya::Render::WakeupAssetUpload();
        }

        inline void Enqueue(Pitaya::Asset::AssetOperate&& operate)
        {
            pendingOperations.Push(std::move(operate));
            Pitaya::Render::WakeupAssetUpload();
        }

        inline void SetTextureCallbacks(void* userData, TextureEntryLookup lookup)
        {
            textureCallbackUserData = userData;
            textureEntryLookup = lookup;
        }

        template<typename Handler>
        inline void Process(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey, Handler&& handler)
        {
            (void)passkey;
            PITAYA_CHECK(Pitaya::Thread::GetIsInThread(Pitaya::Thread::ThreadType::Render));
            if (renderOperations.empty()) { renderOperations = pendingOperations.PopN(5); }

            Pitaya::Core::InvokeWithTimeBudget(
                [this]() ->bool { return renderOperations.empty(); },
                [this, &handler]()
                {
                    std::visit(
                        [this, &handler](auto& operation)
                        {
                            using Operation = std::remove_cvref_t<decltype(operation)>;
                            if constexpr (std::is_same_v<Operation, Pitaya::Import::Texture2DImportResult>)
                            {
                                ProcessTexture2D(operation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::TextureCubemapImportResult>)
                            {
                                ProcessTextureCubemap(operation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Asset::Texture2DUnloadRequire>)
                            {
                                ProcessTexture2DUnload(operation);
                            }
                            else
                            {
                                handler(operation);
                            }
                        },
                        renderOperations.front().Data);
                    renderOperations.pop();
                },
                std::chrono::milliseconds(2));
        }

        inline bool HasPending(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey) const
        {
            (void)passkey;
            return !renderOperations.empty() || !pendingOperations.Empty();
        }

    private:
        inline TextureEntry* FindTextureEntry(Pitaya::Core::GUID guid) const
        {
            return textureEntryLookup ? textureEntryLookup(textureCallbackUserData, guid) : nullptr;
        }

        inline void ProcessTexture2D(Pitaya::Import::Texture2DImportResult& result)
        {
            TextureEntry* entry = FindTextureEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("texture asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("texture asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* texture = entry->Data.load(std::memory_order_acquire);
            if (!texture)
            {
                Pitaya::Log::Error("texture asset data is empty, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            const Pitaya::GPU::PixelFormat format = Pitaya::Asset::TextureUsageToPixelFormat(result.Usage);
            const auto handle = Pitaya::GPU::CreateTexture2D(
                result.Data.data(), result.Width, result.Height, format, result.IsGenerateMipmap, result.isNearest);
            if (!handle)
            {
                Pitaya::Log::Error("texture GPU resource creation failed, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyTexture2D(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted texture GPU resource, GUID:{}", result.GUID);
                }
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            texture->Texture2DHandle = handle;
            texture->Type = Pitaya::GPU::TextureType::Texture2D;
            texture->Usage = result.Usage;
            Pitaya::Log::Info("successfully created GPU resources for texture2D GUID:{}", result.GUID);
            entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
        }

        inline void ProcessTextureCubemap(Pitaya::Import::TextureCubemapImportResult& result)
        {
            TextureEntry* entry = FindTextureEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("texture cubemap asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("texture cubemap asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* texture = entry->Data.load(std::memory_order_acquire);
            if (!texture)
            {
                Pitaya::Log::Error("texture cubemap asset data is empty, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            const void* data[6] = {};
            for (size_t index = 0; index < 6; ++index) { data[index] = result.Data[index].data(); }
            const Pitaya::GPU::PixelFormat format = Pitaya::Asset::TextureUsageToPixelFormat(result.Usage);
            const auto handle = Pitaya::GPU::CreateTextureCubemap(
                data, result.Width, result.Height, format, result.IsGenerateMipmap, result.isNearest);
            if (!handle)
            {
                Pitaya::Log::Error("texture cubemap GPU resource creation failed, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyTextureCubemap(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted texture cubemap GPU resource, GUID:{}", result.GUID);
                }
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            texture->TextureCubemapHandle = handle;
            texture->Type = Pitaya::GPU::TextureType::TextureCubemap;
            texture->Usage = result.Usage;
            Pitaya::Log::Info("successfully created GPU resources for texture cubemap GUID:{}", result.GUID);
            entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
        }

        inline void ProcessTexture2DUnload(Pitaya::Asset::Texture2DUnloadRequire& operation)
        {
            Pitaya::GPU::DestroyTexture2D(operation.Texture2DHandle);
        }

    private:
        void* textureCallbackUserData = nullptr;
        TextureEntryLookup textureEntryLookup = nullptr;
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> pendingOperations;
        std::queue<Pitaya::Asset::AssetOperate> renderOperations;
    };
}
