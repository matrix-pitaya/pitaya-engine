#pragma once

namespace Pitaya::Render
{
    class Renderer;
}

#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/FuncTable.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/RenderTarget.h>
#include<Asset/Common/Shader.h>
#include<Asset/Common/SkyBox.h>
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
#include<GPU/Frontend/Buffer/VertexArray.h>
#include<GPU/Frontend/Shader/Shader.h>
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
        using ShaderEntry = Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry;
        using ShaderEntryLookup = ShaderEntry* (*)(void*, Pitaya::Core::GUID);
        using MeshEntry = Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry;
        using MeshEntryLookup = MeshEntry* (*)(void*, Pitaya::Core::GUID);
        using RenderTargetEntry = Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry;
        using RenderTargetEntryLookup = RenderTargetEntry* (*)(void*, Pitaya::Core::GUID);
        using SkyBoxEntry = Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry;
        using SkyBoxEntryLookup = SkyBoxEntry* (*)(void*, Pitaya::Core::GUID);

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

        inline void EnqueueUnload(Pitaya::Asset::AssetOperate&& operate)
        {
            // 卸载命令单独排队。Process 会先保留一个渲染循环，再在下一次循环
            // 的 OpenGL 上下文中销毁句柄，保证当前帧已经完成提交和 fence。
            pendingUnloadOperations.Push(std::move(operate));
            Pitaya::Render::WakeupAssetUpload();
        }

        inline void SetTextureCallbacks(void* userData, TextureEntryLookup lookup)
        {
            textureCallbackUserData = userData;
            textureEntryLookup = lookup;
        }

        inline void SetShaderCallbacks(void* userData, ShaderEntryLookup lookup)
        {
            shaderCallbackUserData = userData;
            shaderEntryLookup = lookup;
        }

        inline void SetMeshCallbacks(void* userData, MeshEntryLookup lookup)
        {
            meshCallbackUserData = userData;
            meshEntryLookup = lookup;
        }

        inline void SetRenderTargetCallbacks(void* userData, RenderTargetEntryLookup lookup)
        {
            renderTargetCallbackUserData = userData;
            renderTargetEntryLookup = lookup;
        }

        inline void SetSkyBoxCallbacks(void* userData, SkyBoxEntryLookup lookup)
        {
            skyBoxCallbackUserData = userData;
            skyBoxEntryLookup = lookup;
        }

        template<typename Handler>
        inline void Process(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey, Handler&& handler)
        {
            (void)passkey;
            PITAYA_CHECK(Pitaya::Thread::GetIsInThread(Pitaya::Thread::ThreadType::Render));

            ProcessDeferredUnloads();
            if (deferredUnloadOperations.empty())
            {
                deferredUnloadOperations = pendingUnloadOperations.PopN(5);
            }
            if (renderOperations.empty()) { renderOperations = pendingOperations.PopN(5); }

            Pitaya::Core::InvokeWithTimeBudget(
                [this]() ->bool { return renderOperations.empty(); },
                [this, &handler]()
                {
                    const uint64_t generation = renderOperations.front().Generation;
                    std::visit(
                        [this, &handler, generation](auto& operation)
                        {
                            using Operation = std::remove_cvref_t<decltype(operation)>;
                            if constexpr (std::is_same_v<Operation, Pitaya::Import::Texture2DImportResult>)
                            {
                                ProcessTexture2D(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::TextureCubemapImportResult>)
                            {
                                ProcessTextureCubemap(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::ShaderImportResult>)
                            {
                                ProcessShader(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::StaticMeshImportResult>)
                            {
                                ProcessStaticMesh(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::SkinnedMeshImportResult>)
                            {
                                ProcessSkinnedMesh(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::RenderTargetImportResult>)
                            {
                                ProcessRenderTarget(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::SkyBoxImportResult>)
                            {
                                ProcessSkyBox(operation, generation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Asset::Texture2DUnloadRequire>)
                            {
                                ProcessTexture2DUnload(operation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Asset::Texture2DArrayUnloadRequire>
                                || std::is_same_v<Operation, Pitaya::Asset::TextureCubemapUnloadRequire>
                                || std::is_same_v<Operation, Pitaya::Asset::ShaderUnloadRequire>
                                || std::is_same_v<Operation, Pitaya::Asset::MeshUnloadRequire>
                                || std::is_same_v<Operation, Pitaya::Asset::RenderTargetUnloadRequire>
                                || std::is_same_v<Operation, Pitaya::Asset::SkyBoxUnloadRequire>)
                            {
                                // Unload commands enter through EnqueueUnload so they can be
                                // deferred for one render loop. They are consumed below.
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
            return !renderOperations.empty() || !deferredUnloadOperations.empty()
                || !pendingOperations.Empty() || !pendingUnloadOperations.Empty();
        }

    private:
        inline void ProcessDeferredUnloads()
        {
            // 只处理上一次 Process 已经转入 deferred 队列的命令。
            // 本次刚提交的卸载命令会至少等待一个渲染循环。
            while (!deferredUnloadOperations.empty())
            {
                auto& operation = deferredUnloadOperations.front();
                std::visit(
                    [this](auto& unload)
                    {
                        using Operation = std::remove_cvref_t<decltype(unload)>;
                        if constexpr (std::is_same_v<Operation, Pitaya::Asset::Texture2DUnloadRequire>)
                        {
                            ProcessTexture2DUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::Texture2DArrayUnloadRequire>)
                        {
                            ProcessTexture2DArrayUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::TextureCubemapUnloadRequire>)
                        {
                            ProcessTextureCubemapUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::ShaderUnloadRequire>)
                        {
                            ProcessShaderUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::MeshUnloadRequire>)
                        {
                            ProcessMeshUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::RenderTargetUnloadRequire>)
                        {
                            ProcessRenderTargetUnload(unload);
                        }
                        else if constexpr (std::is_same_v<Operation, Pitaya::Asset::SkyBoxUnloadRequire>)
                        {
                            ProcessSkyBoxUnload(unload);
                        }
                    },
                    operation.Data);
                deferredUnloadOperations.pop();
            }
        }

        inline TextureEntry* FindTextureEntry(Pitaya::Core::GUID guid) const
        {
            return textureEntryLookup ? textureEntryLookup(textureCallbackUserData, guid) : nullptr;
        }

        inline void ProcessTexture2D(Pitaya::Import::Texture2DImportResult& result, uint64_t generation)
        {
            TextureEntry* entry = FindTextureEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("texture asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (!entry->IsLoadGeneration(generation)) { return; }
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
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            const Pitaya::GPU::PixelFormat format = Pitaya::Asset::TextureUsageToPixelFormat(result.Usage);
            const auto handle = Pitaya::GPU::CreateTexture2D(
                result.Data.data(), result.Width, result.Height, format, result.IsGenerateMipmap, result.isNearest);
            if (!handle)
            {
                Pitaya::Log::Error("texture GPU resource creation failed, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyTexture2D(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted texture GPU resource, GUID:{}", result.GUID);
                }
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            texture->Texture2DHandle = handle;
            texture->Type = Pitaya::GPU::TextureType::Texture2D;
            texture->Usage = result.Usage;
            Pitaya::Log::Info("successfully created GPU resources for texture2D GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline void ProcessTextureCubemap(Pitaya::Import::TextureCubemapImportResult& result, uint64_t generation)
        {
            TextureEntry* entry = FindTextureEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("texture cubemap asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (!entry->IsLoadGeneration(generation)) { return; }
            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("texture cubemap asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* texture = entry->Data.load(std::memory_order_acquire);
            if (!texture)
            {
                Pitaya::Log::Error("texture cubemap asset data is empty, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
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
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyTextureCubemap(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted texture cubemap GPU resource, GUID:{}", result.GUID);
                }
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            texture->TextureCubemapHandle = handle;
            texture->Type = Pitaya::GPU::TextureType::TextureCubemap;
            texture->Usage = result.Usage;
            Pitaya::Log::Info("successfully created GPU resources for texture cubemap GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline void ProcessTexture2DUnload(Pitaya::Asset::Texture2DUnloadRequire& operation)
        {
            if (operation.Texture2DHandle) { Pitaya::GPU::DestroyTexture2D(operation.Texture2DHandle); }
        }

        inline void ProcessTexture2DArrayUnload(Pitaya::Asset::Texture2DArrayUnloadRequire& operation)
        {
            if (operation.Texture2DArrayHandle) { Pitaya::GPU::DestroyTexture2DArray(operation.Texture2DArrayHandle); }
        }

        inline void ProcessTextureCubemapUnload(Pitaya::Asset::TextureCubemapUnloadRequire& operation)
        {
            if (operation.TextureCubemapHandle) { Pitaya::GPU::DestroyTextureCubemap(operation.TextureCubemapHandle); }
        }

        inline void ProcessShaderUnload(Pitaya::Asset::ShaderUnloadRequire& operation)
        {
            if (operation.ShaderHandle) { Pitaya::GPU::DestroyShader(operation.ShaderHandle); }
        }

        inline void ProcessMeshUnload(Pitaya::Asset::MeshUnloadRequire& operation)
        {
            if (operation.VertexArrayHandle) { Pitaya::GPU::DestroyVertexArray(operation.VertexArrayHandle); }
            if (operation.VertexBufferHandle) { Pitaya::GPU::DestroyVertexBuffer(operation.VertexBufferHandle); }
            if (operation.IndexBufferHandle) { Pitaya::GPU::DestroyIndexBuffer(operation.IndexBufferHandle); }
        }

        inline void ProcessRenderTargetUnload(Pitaya::Asset::RenderTargetUnloadRequire& operation)
        {
            if (operation.SceneFrameBufferHandle) { Pitaya::GPU::DestroyFrameBuffer(operation.SceneFrameBufferHandle); }
            if (operation.PingPongFrameBufferHandles[0]) { Pitaya::GPU::DestroyFrameBuffer(operation.PingPongFrameBufferHandles[0]); }
            if (operation.PingPongFrameBufferHandles[1]) { Pitaya::GPU::DestroyFrameBuffer(operation.PingPongFrameBufferHandles[1]); }
            if (operation.FinalFrameBufferHandle) { Pitaya::GPU::DestroyFrameBuffer(operation.FinalFrameBufferHandle); }
        }

        inline void ProcessSkyBoxUnload(Pitaya::Asset::SkyBoxUnloadRequire& operation)
        {
            if (operation.EquirectHandle) { Pitaya::GPU::DestroyTexture2D(operation.EquirectHandle); }
            if (operation.EnvCubemapHandle) { Pitaya::GPU::DestroyTextureCubemap(operation.EnvCubemapHandle); }
            if (operation.IrradianceHandle) { Pitaya::GPU::DestroyTextureCubemap(operation.IrradianceHandle); }
            if (operation.PrefilteredHandle) { Pitaya::GPU::DestroyTextureCubemap(operation.PrefilteredHandle); }
        }

        inline ShaderEntry* FindShaderEntry(Pitaya::Core::GUID guid) const
        {
            return shaderEntryLookup ? shaderEntryLookup(shaderCallbackUserData, guid) : nullptr;
        }

        inline void ProcessShader(Pitaya::Import::ShaderImportResult& result, uint64_t generation)
        {
            ShaderEntry* entry = FindShaderEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("shader asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (!entry->IsLoadGeneration(generation)) { return; }
            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("shader asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* shader = entry->Data.load(std::memory_order_acquire);
            if (!shader)
            {
                Pitaya::Log::Error("shader asset data is empty, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            Pitaya::Core::SlotMap<Pitaya::GPU::Shader>::Handle handle =
                Pitaya::Core::SlotMap<Pitaya::GPU::Shader>::Handle::Invalid;
            if (result.Type == Pitaya::GPU::Shader::VF && !result.VertexSource.empty() && !result.FragmentSource.empty())
            {
                handle = Pitaya::GPU::CreateShader(result.VertexSource.c_str(), result.VertexSource.size(),
                    result.FragmentSource.c_str(), result.FragmentSource.size());
            }
            else if (result.Type == Pitaya::GPU::Shader::VFG && !result.VertexSource.empty()
                && !result.FragmentSource.empty() && !result.GeometrySource.empty())
            {
                handle = Pitaya::GPU::CreateShader(result.VertexSource.c_str(), result.VertexSource.size(),
                    result.FragmentSource.c_str(), result.FragmentSource.size(), result.GeometrySource.c_str(),
                    result.GeometrySource.size());
            }
            else
            {
                Pitaya::Log::Error("invalid shader CPU result, GUID:{}", result.GUID);
            }

            if (!handle)
            {
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyShader(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted shader GPU resource, GUID:{}", result.GUID);
                }
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            shader->ShaderHandle = handle;
            Pitaya::Log::Info("successfully created GPU resources for shader GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline MeshEntry* FindMeshEntry(Pitaya::Core::GUID guid) const
        {
            return meshEntryLookup ? meshEntryLookup(meshCallbackUserData, guid) : nullptr;
        }

        inline bool BeginMeshUpload(Pitaya::Core::GUID guid, MeshEntry*& entry, const char* typeName, uint64_t generation)
        {
            entry = FindMeshEntry(guid);
            if (!entry)
            {
                Pitaya::Log::Error("{} asset entry not found, GUID:{}", typeName, guid);
                return false;
            }
            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("{} asset marked as unload, GUID:{}", typeName, guid);
                return false;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            return true;
        }

        inline bool CreateMeshBuffers(const std::vector<float>& vertices, const std::vector<uint32_t>& indices,
            const Pitaya::GPU::BufferLayout& layout,
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle& vao,
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle& vbo,
            Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle& ibo)
        {
            vao = Pitaya::GPU::CreateVertexArray();
            vbo = Pitaya::GPU::CreateVertexBuffer(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
            ibo = Pitaya::GPU::CreateIndexBuffer(indices.data(), static_cast<uint32_t>(indices.size()));
            if (!vao || !vbo || !ibo || !Pitaya::GPU::LinkVertexArray(vao, vbo, ibo, layout))
            {
                if (vao) { Pitaya::GPU::DestroyVertexArray(vao); }
                if (vbo) { Pitaya::GPU::DestroyVertexBuffer(vbo); }
                if (ibo) { Pitaya::GPU::DestroyIndexBuffer(ibo); }
                vao = Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle::Invalid;
                vbo = Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle::Invalid;
                ibo = Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle::Invalid;
                return false;
            }
            return true;
        }

        inline void DestroyMeshBuffers(Pitaya::Asset::Mesh& mesh)
        {
            if (mesh.VertexArrayHandle) { Pitaya::GPU::DestroyVertexArray(mesh.VertexArrayHandle); }
            if (mesh.VertexBufferHandle) { Pitaya::GPU::DestroyVertexBuffer(mesh.VertexBufferHandle); }
            if (mesh.IndexBufferHandle) { Pitaya::GPU::DestroyIndexBuffer(mesh.IndexBufferHandle); }
            mesh.VertexArrayHandle = Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle::Invalid;
            mesh.VertexBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle::Invalid;
            mesh.IndexBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle::Invalid;
        }

        inline void ProcessStaticMesh(Pitaya::Import::StaticMeshImportResult& result, uint64_t generation)
        {
            MeshEntry* entry = nullptr;
            if (!BeginMeshUpload(result.GUID, entry, "static mesh", generation)) { return; }
            if (!result.IsValid || result.Vertices.empty() || result.Indices.empty())
            {
                Pitaya::Log::Error("invalid static mesh CPU result, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            auto* mesh = entry->Data.load(std::memory_order_acquire);
            if (!mesh)
            {
                Pitaya::Log::Error("static mesh asset data is empty, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle vao;
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle vbo;
            Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle ibo;
            if (!CreateMeshBuffers(result.Vertices, result.Indices, result.VertexLayout, vao, vbo, ibo)
                || !entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (vao) { Pitaya::GPU::DestroyVertexArray(vao); }
                if (vbo) { Pitaya::GPU::DestroyVertexBuffer(vbo); }
                if (ibo) { Pitaya::GPU::DestroyIndexBuffer(ibo); }
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            DestroyMeshBuffers(*mesh);
            mesh->VertexArrayHandle = vao;
            mesh->VertexBufferHandle = vbo;
            mesh->IndexBufferHandle = ibo;
            mesh->BoundingBox = result.BoundingBox;
            mesh->SubMeshs = std::move(result.SubMeshs);
            mesh->Materials.clear();
            for (const auto& materialGUID : result.MaterialGUIDs)
            {
                mesh->Materials.emplace_back(Pitaya::Asset::LoadAsset<Pitaya::Asset::Material>(materialGUID));
            }
            mesh->Type = Pitaya::GPU::MeshType::Static;
            Pitaya::Log::Info("successfully created GPU resources for static mesh GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline void ProcessSkinnedMesh(Pitaya::Import::SkinnedMeshImportResult& result, uint64_t generation)
        {
            MeshEntry* entry = nullptr;
            if (!BeginMeshUpload(result.GUID, entry, "skinned mesh", generation)) { return; }
            if (!result.IsValid || result.Vertices.empty() || result.Indices.empty())
            {
                Pitaya::Log::Error("invalid skinned mesh CPU result, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            auto* mesh = entry->Data.load(std::memory_order_acquire);
            if (!mesh)
            {
                Pitaya::Log::Error("skinned mesh asset data is empty, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle vao;
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle vbo;
            Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle ibo;
            if (!CreateMeshBuffers(result.Vertices, result.Indices, result.VertexLayout, vao, vbo, ibo)
                || !entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (vao) { Pitaya::GPU::DestroyVertexArray(vao); }
                if (vbo) { Pitaya::GPU::DestroyVertexBuffer(vbo); }
                if (ibo) { Pitaya::GPU::DestroyIndexBuffer(ibo); }
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            DestroyMeshBuffers(*mesh);
            mesh->VertexArrayHandle = vao;
            mesh->VertexBufferHandle = vbo;
            mesh->IndexBufferHandle = ibo;
            mesh->SubMeshs = std::move(result.SubMeshs);
            mesh->BoneInverseMatrices = std::move(result.BoneInverseMatrices);
            mesh->BoneCount = result.BoneCount;
            mesh->BoundingBox = result.BoundingBox;
            mesh->Materials.clear();
            for (const auto& materialGUID : result.MaterialGUIDs)
            {
                mesh->Materials.emplace_back(Pitaya::Asset::LoadAsset<Pitaya::Asset::Material>(materialGUID));
            }
            mesh->Type = Pitaya::GPU::MeshType::SkinnedMesh;
            Pitaya::Log::Info("successfully created GPU resources for skinned mesh GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline RenderTargetEntry* FindRenderTargetEntry(Pitaya::Core::GUID guid) const
        {
            return renderTargetEntryLookup ? renderTargetEntryLookup(renderTargetCallbackUserData, guid) : nullptr;
        }

        inline void DestroyRenderTargetBuffers(Pitaya::Asset::RenderTarget& renderTarget)
        {
            Pitaya::GPU::DestroyFrameBuffer(renderTarget.SceneFrameBufferHandle);
            Pitaya::GPU::DestroyFrameBuffer(renderTarget.PingPongFrameBufferHandles[0]);
            Pitaya::GPU::DestroyFrameBuffer(renderTarget.PingPongFrameBufferHandles[1]);
            Pitaya::GPU::DestroyFrameBuffer(renderTarget.FinalFrameBufferHandle);
            renderTarget.SceneFrameBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
            renderTarget.PingPongFrameBufferHandles[0] = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
            renderTarget.PingPongFrameBufferHandles[1] = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
            renderTarget.FinalFrameBufferHandle = Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle::Invalid;
        }

        inline void ProcessRenderTarget(Pitaya::Import::RenderTargetImportResult& result, uint64_t generation)
        {
            RenderTargetEntry* entry = FindRenderTargetEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("render target asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("render target asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* renderTarget = entry->Data.load(std::memory_order_acquire);
            if (!renderTarget)
            {
                Pitaya::Log::Error("render target asset data is empty, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            const auto sceneHandle = Pitaya::GPU::CreateFrameBuffer(renderTarget->SceneFrameBufferSpecification);
            const auto pingPongHandle0 = Pitaya::GPU::CreateFrameBuffer(renderTarget->PingPongFrameBufferSpecification);
            const auto pingPongHandle1 = Pitaya::GPU::CreateFrameBuffer(renderTarget->PingPongFrameBufferSpecification);
            const auto finalHandle = Pitaya::GPU::CreateFrameBuffer(renderTarget->FinalFrameBufferSpecification);
            if (!sceneHandle || !pingPongHandle0 || !pingPongHandle1 || !finalHandle)
            {
                Pitaya::GPU::DestroyFrameBuffer(sceneHandle);
                Pitaya::GPU::DestroyFrameBuffer(pingPongHandle0);
                Pitaya::GPU::DestroyFrameBuffer(pingPongHandle1);
                Pitaya::GPU::DestroyFrameBuffer(finalHandle);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::GPU::DestroyFrameBuffer(sceneHandle);
                Pitaya::GPU::DestroyFrameBuffer(pingPongHandle0);
                Pitaya::GPU::DestroyFrameBuffer(pingPongHandle1);
                Pitaya::GPU::DestroyFrameBuffer(finalHandle);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            DestroyRenderTargetBuffers(*renderTarget);
            renderTarget->SceneFrameBufferHandle = sceneHandle;
            renderTarget->PingPongFrameBufferHandles[0] = pingPongHandle0;
            renderTarget->PingPongFrameBufferHandles[1] = pingPongHandle1;
            renderTarget->FinalFrameBufferHandle = finalHandle;
            Pitaya::Log::Info("successfully created GPU resources for render target GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

        inline SkyBoxEntry* FindSkyBoxEntry(Pitaya::Core::GUID guid) const
        {
            return skyBoxEntryLookup ? skyBoxEntryLookup(skyBoxCallbackUserData, guid) : nullptr;
        }

        inline void DestroySkyBoxTextures(Pitaya::Asset::SkyBox& skyBox)
        {
            Pitaya::GPU::DestroyTexture2D(skyBox.EquirectHandle);
            Pitaya::GPU::DestroyTextureCubemap(skyBox.EnvCubemapHandle);
            Pitaya::GPU::DestroyTextureCubemap(skyBox.IrradianceHandle);
            Pitaya::GPU::DestroyTextureCubemap(skyBox.PrefilteredHandle);
            skyBox.EquirectHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle::Invalid;
            skyBox.EnvCubemapHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
            skyBox.IrradianceHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
            skyBox.PrefilteredHandle = Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle::Invalid;
        }

        inline void ProcessSkyBox(Pitaya::Import::SkyBoxImportResult& result, uint64_t generation)
        {
            SkyBoxEntry* entry = FindSkyBoxEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("skybox asset entry not found, GUID:{}", result.GUID);
                return;
            }
            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                Pitaya::Log::Error("skybox asset marked as unload, GUID:{}", result.GUID);
                return;
            }
            entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);

            auto* skyBox = entry->Data.load(std::memory_order_acquire);
            if (!skyBox || result.Width <= 0 || result.Height <= 0 || result.Data.empty())
            {
                Pitaya::Log::Error("invalid skybox CPU result, GUID:{}", result.GUID);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            const auto equirectHandle = Pitaya::GPU::CreateTexture2D(result.Data.data(), result.Width, result.Height,
                Pitaya::GPU::PixelFormat::RGBA16F, false, false);
            const auto envHandle = Pitaya::GPU::CreateEmptyTextureCubemap(512, 10, Pitaya::GPU::PixelFormat::RGBA16F);
            const auto irradianceHandle = Pitaya::GPU::CreateEmptyTextureCubemap(32, 1, Pitaya::GPU::PixelFormat::RGBA16F);
            const auto prefilteredHandle = Pitaya::GPU::CreateEmptyTextureCubemap(256, 6, Pitaya::GPU::PixelFormat::RGBA16F);
            if (!equirectHandle || !envHandle || !irradianceHandle || !prefilteredHandle)
            {
                Pitaya::GPU::DestroyTexture2D(equirectHandle);
                Pitaya::GPU::DestroyTextureCubemap(envHandle);
                Pitaya::GPU::DestroyTextureCubemap(irradianceHandle);
                Pitaya::GPU::DestroyTextureCubemap(prefilteredHandle);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            const Pitaya::Render::IBLBakeInput bakeInput{ equirectHandle, envHandle, irradianceHandle, prefilteredHandle };
            if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload) || !Pitaya::Render::BakeIBL(bakeInput))
            {
                Pitaya::GPU::DestroyTexture2D(equirectHandle);
                Pitaya::GPU::DestroyTextureCubemap(envHandle);
                Pitaya::GPU::DestroyTextureCubemap(irradianceHandle);
                Pitaya::GPU::DestroyTextureCubemap(prefilteredHandle);
                if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); }
                return;
            }

            DestroySkyBoxTextures(*skyBox);
            skyBox->EquirectHandle = equirectHandle;
            skyBox->EnvCubemapHandle = envHandle;
            skyBox->IrradianceHandle = irradianceHandle;
            skyBox->PrefilteredHandle = prefilteredHandle;
            Pitaya::Log::Info("successfully created GPU resources for skybox GUID:{}", result.GUID);
            if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading); }
        }

    private:
        void* textureCallbackUserData = nullptr;
        TextureEntryLookup textureEntryLookup = nullptr;
        void* shaderCallbackUserData = nullptr;
        ShaderEntryLookup shaderEntryLookup = nullptr;
        void* meshCallbackUserData = nullptr;
        MeshEntryLookup meshEntryLookup = nullptr;
        void* renderTargetCallbackUserData = nullptr;
        RenderTargetEntryLookup renderTargetEntryLookup = nullptr;
        void* skyBoxCallbackUserData = nullptr;
        SkyBoxEntryLookup skyBoxEntryLookup = nullptr;
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> pendingOperations;
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> pendingUnloadOperations;
        std::queue<Pitaya::Asset::AssetOperate> renderOperations;
        std::queue<Pitaya::Asset::AssetOperate> deferredUnloadOperations;
    };
}
