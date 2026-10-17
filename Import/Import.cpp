#include<Import/Import.h>
#include<Import/ShaderImporter/ShaderImporter.h>
#include<Import/MeshImporter/Backend/AssimpMeshImporter.h>
#include<Import/TextureImporter/Backend/StbImage/StbImageTextureImporter.h>
#include<Log/Common/FuncTable.h>

#include<memory>

namespace
{
    inline std::unique_ptr<Pitaya::Import::TextureImporter> CreateTextureImporter(Pitaya::Import::TextureAPI api) noexcept
    {
        switch (api)
        {
            case Pitaya::Import::TextureAPI::StbImage:
                return std::make_unique<Pitaya::Import::StbImageTextureImporter>();

            case Pitaya::Import::TextureAPI::Invalid:
                Pitaya::Log::Warning("unknown texture import API, create StbImageTexture2D importer");
                return std::make_unique<Pitaya::Import::StbImageTextureImporter>();

            default:
                Pitaya::Log::Warning("unknown texture import API, create StbImageTexture2D importer");
                return std::make_unique<Pitaya::Import::StbImageTextureImporter>();
        }
    }
    inline std::unique_ptr<Pitaya::Import::MeshImporter> CreateMeshImporter(Pitaya::Import::MeshAPI api)
    {
        switch (api)
        {
            case Pitaya::Import::MeshAPI::Assimp:
                return std::make_unique<Pitaya::Import::AssimpMeshImporter>();
        
            case Pitaya::Import::MeshAPI::Invalid:
                Pitaya::Log::Warning("unknwon mesh api, create assimp mesh importer");
                return std::make_unique<Pitaya::Import::AssimpMeshImporter>();

            default:
                Pitaya::Log::Warning("unknwon mesh api, create assimp mesh importer");
                return std::make_unique<Pitaya::Import::AssimpMeshImporter>();
        }
    }
}

bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, bool isFlipVerticallyOnLoad, bool isGenerateMipmap, Pitaya::Import::Texture2DImportResult& out, Pitaya::Import::TextureAPI api)
{
    return CreateTextureImporter(api)->Import(guid, file, isFlipVerticallyOnLoad, isGenerateMipmap, out);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, const std::vector<std::filesystem::path>& paths, bool isFlipVerticallyOnLoad, bool isGenerateMipmap, Pitaya::Import::TextureCubemapImportResult& out, Pitaya::Import::TextureAPI api)
{
    return CreateTextureImporter(api)->Import(guid, file, paths, isFlipVerticallyOnLoad, isGenerateMipmap, out);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, Pitaya::Import::ShaderImportResult& out)
{
    return ShaderImporter().Import(guid, folder, vertexPath, fragmentPath, out);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath, Pitaya::Import::ShaderImportResult& out)
{
    return ShaderImporter().Import(guid, folder, vertexPath, fragmentPath, geometryPath, out);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, Pitaya::Import::MeshPreloadResult& out, Pitaya::Import::MeshAPI api, const Pitaya::Import::ImportContext* context)
{
	return CreateMeshImporter(api)->Import(guid, file, out, context);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::StaticMeshImportResult& out, Pitaya::Import::MeshAPI api, const Pitaya::Import::ImportContext* context)
{
	return CreateMeshImporter(api)->Import(guid, file, preload, out, context);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::SkinnedMeshImportResult& out, Pitaya::Import::MeshAPI api, const Pitaya::Import::ImportContext* context)
{
	return CreateMeshImporter(api)->Import(guid, file, preload, out, context);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, bool isFlipVerticallyOnLoad, Pitaya::Import::SkyBoxImportResult& out)
{
    return CreateTextureImporter(Pitaya::Import::TextureAPI::StbImage)->Import(guid, file, isFlipVerticallyOnLoad, out);
}
bool Pitaya::Import::Import(Pitaya::Core::GUID guid, const void* data, size_t size, bool isFlipVerticallyOnLoad, Pitaya::Import::SkyBoxImportResult& out)
{
    return CreateTextureImporter(Pitaya::Import::TextureAPI::StbImage)->Import(guid, data, size, isFlipVerticallyOnLoad, out);
}

