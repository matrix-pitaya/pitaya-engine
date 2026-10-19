#pragma once

namespace Pitaya::Render
{
    class Renderer;
}

#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/FuncTable.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/Shader.h>
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
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::ShaderImportResult>)
                            {
                                ProcessShader(operation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::StaticMeshImportResult>)
                            {
                                ProcessStaticMesh(operation);
                            }
                            else if constexpr (std::is_same_v<Operation, Pitaya::Import::SkinnedMeshImportResult>)
                            {
                                ProcessSkinnedMesh(operation);
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

        inline ShaderEntry* FindShaderEntry(Pitaya::Core::GUID guid) const
        {
            return shaderEntryLookup ? shaderEntryLookup(shaderCallbackUserData, guid) : nullptr;
        }

        inline void ProcessShader(Pitaya::Import::ShaderImportResult& result)
        {
            ShaderEntry* entry = FindShaderEntry(result.GUID);
            if (!entry)
            {
                Pitaya::Log::Error("shader asset entry not found, GUID:{}", result.GUID);
                return;
            }
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
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (!Pitaya::GPU::DestroyShader(handle))
                {
                    Pitaya::Log::Error("failed to destroy aborted shader GPU resource, GUID:{}", result.GUID);
                }
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            shader->ShaderHandle = handle;
            Pitaya::Log::Info("successfully created GPU resources for shader GUID:{}", result.GUID);
            entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
        }

        inline MeshEntry* FindMeshEntry(Pitaya::Core::GUID guid) const
        {
            return meshEntryLookup ? meshEntryLookup(meshCallbackUserData, guid) : nullptr;
        }

        inline bool BeginMeshUpload(Pitaya::Core::GUID guid, MeshEntry*& entry, const char* typeName)
        {
            entry = FindMeshEntry(guid);
            if (!entry)
            {
                Pitaya::Log::Error("{} asset entry not found, GUID:{}", typeName, guid);
                return false;
            }
            if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
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
            const auto vao = mesh.VertexArrayHandle;
            if (!vao) { return; }

            Pitaya::GPU::VertexArray vertexArray;
            if (Pitaya::GPU::GetVertexArray(vao, vertexArray))
            {
                if (vertexArray.VertexBufferHandle) { Pitaya::GPU::DestroyVertexBuffer(vertexArray.VertexBufferHandle); }
                if (vertexArray.IndexBufferHandle) { Pitaya::GPU::DestroyIndexBuffer(vertexArray.IndexBufferHandle); }
            }
            Pitaya::GPU::DestroyVertexArray(vao);
            mesh.VertexArrayHandle = Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle::Invalid;
        }

        inline void ProcessStaticMesh(Pitaya::Import::StaticMeshImportResult& result)
        {
            MeshEntry* entry = nullptr;
            if (!BeginMeshUpload(result.GUID, entry, "static mesh")) { return; }
            if (!result.IsValid || result.Vertices.empty() || result.Indices.empty())
            {
                Pitaya::Log::Error("invalid static mesh CPU result, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            auto* mesh = entry->Data.load(std::memory_order_acquire);
            if (!mesh)
            {
                Pitaya::Log::Error("static mesh asset data is empty, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle vao;
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle vbo;
            Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle ibo;
            if (!CreateMeshBuffers(result.Vertices, result.Indices, result.VertexLayout, vao, vbo, ibo)
                || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (vao) { Pitaya::GPU::DestroyVertexArray(vao); }
                if (vbo) { Pitaya::GPU::DestroyVertexBuffer(vbo); }
                if (ibo) { Pitaya::GPU::DestroyIndexBuffer(ibo); }
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            DestroyMeshBuffers(*mesh);
            mesh->VertexArrayHandle = vao;
            mesh->BoundingBox = result.BoundingBox;
            mesh->SubMeshs = std::move(result.SubMeshs);
            mesh->Materials.clear();
            for (const auto& materialGUID : result.MaterialGUIDs)
            {
                mesh->Materials.emplace_back(Pitaya::Asset::LoadAsset<Pitaya::Asset::Material>(materialGUID));
            }
            mesh->Type = Pitaya::GPU::MeshType::Static;
            Pitaya::Log::Info("successfully created GPU resources for static mesh GUID:{}", result.GUID);
            entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
        }

        inline void ProcessSkinnedMesh(Pitaya::Import::SkinnedMeshImportResult& result)
        {
            MeshEntry* entry = nullptr;
            if (!BeginMeshUpload(result.GUID, entry, "skinned mesh")) { return; }
            if (!result.IsValid || result.Vertices.empty() || result.Indices.empty())
            {
                Pitaya::Log::Error("invalid skinned mesh CPU result, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            auto* mesh = entry->Data.load(std::memory_order_acquire);
            if (!mesh)
            {
                Pitaya::Log::Error("skinned mesh asset data is empty, GUID:{}", result.GUID);
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle vao;
            Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle vbo;
            Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle ibo;
            if (!CreateMeshBuffers(result.Vertices, result.Indices, result.VertexLayout, vao, vbo, ibo)
                || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                if (vao) { Pitaya::GPU::DestroyVertexArray(vao); }
                if (vbo) { Pitaya::GPU::DestroyVertexBuffer(vbo); }
                if (ibo) { Pitaya::GPU::DestroyIndexBuffer(ibo); }
                entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
                return;
            }

            DestroyMeshBuffers(*mesh);
            mesh->VertexArrayHandle = vao;
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
            entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
        }

    private:
        void* textureCallbackUserData = nullptr;
        TextureEntryLookup textureEntryLookup = nullptr;
        void* shaderCallbackUserData = nullptr;
        ShaderEntryLookup shaderEntryLookup = nullptr;
        void* meshCallbackUserData = nullptr;
        MeshEntryLookup meshEntryLookup = nullptr;
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> pendingOperations;
        std::queue<Pitaya::Asset::AssetOperate> renderOperations;
    };
}
