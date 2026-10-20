#pragma once

#include<Core/Identifier/GUID.h>
#include<Import/Common/ImportContext.h>
#include<Import/Common/ImportResult.h>

#include<filesystem>
#include<vector>

namespace Pitaya::Asset
{
    // ImportPipeline 只负责把源文件转换为 CPU ImportResult。
    // 它不访问 AssetEntry、AssetDatabase 或 GPU；生命周期和调度由 AssetHub
    // 兼容 facade 以及后续 RuntimeAssetManager 负责。
    class ImportPipeline final
    {
    public:
        ImportPipeline() = default;
        ~ImportPipeline() = default;

        ImportPipeline(const ImportPipeline&) = delete;
        ImportPipeline& operator=(const ImportPipeline&) = delete;
        ImportPipeline(ImportPipeline&&) = delete;
        ImportPipeline& operator=(ImportPipeline&&) = delete;

        void SetContext(Pitaya::Import::ImportContext context) noexcept;

        bool ImportTexture2D(Pitaya::Core::GUID guid, const std::filesystem::path& path,
            bool flipVertically, bool generateMipmap, Pitaya::Import::Texture2DImportResult& result) const;
        bool ImportTextureCubemap(Pitaya::Core::GUID guid, const std::filesystem::path& firstPath,
            const std::vector<std::filesystem::path>& paths, bool flipVertically, bool generateMipmap,
            Pitaya::Import::TextureCubemapImportResult& result) const;
        bool ImportShader(Pitaya::Core::GUID guid, const std::filesystem::path& folder,
            const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath,
            Pitaya::Import::ShaderImportResult& result) const;
        bool ImportShader(Pitaya::Core::GUID guid, const std::filesystem::path& folder,
            const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath,
            const std::filesystem::path& geometryPath, Pitaya::Import::ShaderImportResult& result) const;
        bool PreloadMesh(Pitaya::Core::GUID guid, const std::filesystem::path& path,
            Pitaya::Import::MeshPreloadResult& result) const;
        bool ImportStaticMesh(Pitaya::Core::GUID guid, const std::filesystem::path& path,
            const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::StaticMeshImportResult& result) const;
        bool ImportSkinnedMesh(Pitaya::Core::GUID guid, const std::filesystem::path& path,
            const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::SkinnedMeshImportResult& result) const;
        bool ImportSkyBox(Pitaya::Core::GUID guid, const std::filesystem::path& path,
            bool flipVertically, Pitaya::Import::SkyBoxImportResult& result) const;
        bool ImportSkyBox(Pitaya::Core::GUID guid, const void* data, size_t size,
            bool flipVertically, Pitaya::Import::SkyBoxImportResult& result) const;

    private:
        Pitaya::Import::ImportContext context;
    };
}
