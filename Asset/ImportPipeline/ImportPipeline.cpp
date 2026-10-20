#include<Asset/ImportPipeline/ImportPipeline.h>

#include<Import/Import.h>

void Pitaya::Asset::ImportPipeline::SetContext(Pitaya::Import::ImportContext value) noexcept
{
    context = value;
}

bool Pitaya::Asset::ImportPipeline::ImportTexture2D(Pitaya::Core::GUID guid,
    const std::filesystem::path& path, bool flipVertically, bool generateMipmap,
    Pitaya::Import::Texture2DImportResult& result) const
{
    return Pitaya::Import::Import(guid, path, flipVertically, generateMipmap, result);
}

bool Pitaya::Asset::ImportPipeline::ImportTextureCubemap(Pitaya::Core::GUID guid,
    const std::filesystem::path& firstPath, const std::vector<std::filesystem::path>& paths,
    bool flipVertically, bool generateMipmap, Pitaya::Import::TextureCubemapImportResult& result) const
{
    return Pitaya::Import::Import(guid, firstPath, paths, flipVertically, generateMipmap, result);
}

bool Pitaya::Asset::ImportPipeline::ImportShader(Pitaya::Core::GUID guid,
    const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, Pitaya::Import::ShaderImportResult& result) const
{
    return Pitaya::Import::Import(guid, folder, vertexPath, fragmentPath, result);
}

bool Pitaya::Asset::ImportPipeline::ImportShader(Pitaya::Core::GUID guid,
    const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath,
    Pitaya::Import::ShaderImportResult& result) const
{
    return Pitaya::Import::Import(guid, folder, vertexPath, fragmentPath, geometryPath, result);
}

bool Pitaya::Asset::ImportPipeline::PreloadMesh(Pitaya::Core::GUID guid,
    const std::filesystem::path& path, Pitaya::Import::MeshPreloadResult& result) const
{
    return Pitaya::Import::Import(guid, path, result, Pitaya::Import::MeshAPI::Assimp, &context);
}

bool Pitaya::Asset::ImportPipeline::ImportStaticMesh(Pitaya::Core::GUID guid,
    const std::filesystem::path& path, const Pitaya::Import::MeshPreloadResult& preload,
    Pitaya::Import::StaticMeshImportResult& result) const
{
    return Pitaya::Import::Import(guid, path, preload, result, Pitaya::Import::MeshAPI::Assimp, &context);
}

bool Pitaya::Asset::ImportPipeline::ImportSkinnedMesh(Pitaya::Core::GUID guid,
    const std::filesystem::path& path, const Pitaya::Import::MeshPreloadResult& preload,
    Pitaya::Import::SkinnedMeshImportResult& result) const
{
    return Pitaya::Import::Import(guid, path, preload, result, Pitaya::Import::MeshAPI::Assimp, &context);
}

bool Pitaya::Asset::ImportPipeline::ImportSkyBox(Pitaya::Core::GUID guid,
    const std::filesystem::path& path, bool flipVertically, Pitaya::Import::SkyBoxImportResult& result) const
{
    return Pitaya::Import::Import(guid, path, flipVertically, result);
}

bool Pitaya::Asset::ImportPipeline::ImportSkyBox(Pitaya::Core::GUID guid,
    const void* data, size_t size, bool flipVertically, Pitaya::Import::SkyBoxImportResult& result) const
{
    return Pitaya::Import::Import(guid, data, size, flipVertically, result);
}
