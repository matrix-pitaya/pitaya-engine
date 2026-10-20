#include<Asset/AssetHub.h>
#include<GPU/Common/FuncTable.h>
#include<GPU/Common/TextureType.h>
#include<GPU/Common/MeshType.h>
#include<GPU/Common/TextureCubemapFace.h>
#include<GPU/Frontend/Shader/Shader.h>
#include<GPU/Frontend/Buffer/VertexArray.h>
#include<GPU/Frontend/Buffer/FrameBuffer.h>
#include<Render/Common/FuncTable.h>
#include<Import/Import.h>
#include<Import/Common/MeshVertex.h>
#include<Core/Utils/File.h>
#include<Core/Utils/System.h>
#include<Application/Built-in.h>

#include<unordered_set>

namespace
{
    const std::unordered_set<std::string> TextureExtensions =
    { ".png", ".jpg", ".jpeg", ".bmp", ".tga",	".gif",				//普通位图
      ".hdr", ".exr" };												//高动态范围图（仍属于2D纹理）
    const std::unordered_map<std::string, Pitaya::GPU::ShaderType> ShaderExtensions =
    { {".vert", Pitaya::GPU::ShaderType::Vertex},{".frag", Pitaya::GPU::ShaderType::Fragment},
      {".geom", Pitaya::GPU::ShaderType::Geometry}, };
    const std::unordered_set<std::string> MeshExtensions =
    { ".obj" };
    const std::unordered_set<std::string> MaterialExtensions =
    { ".mat" };
    const std::unordered_set<std::string> RenderTargetExtensions =
    { ".rt" };

    bool TransformImportPath(void* userData, const std::filesystem::path& inputPath,
        const std::filesystem::path& basePath, std::filesystem::path& outPath)
    {
        auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
        return assetHub && assetHub->TransformToVirtualPath(inputPath, basePath, outPath);
    }

    bool ResolveImportGUID(void* userData, const std::filesystem::path& path,
        Pitaya::Core::GUID& outGUID)
    {
        auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
        return assetHub && assetHub->GetAssetGUIDByPath(path, outGUID);
    }

    bool RegisterImportFile(void* userData, const std::filesystem::path& inputPath,
        const std::filesystem::path& basePath, std::filesystem::path& outVirtualPath,
        Pitaya::Core::GUID& outGUID)
    {
        auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
        return assetHub && assetHub->RegisterExternalFile(inputPath, basePath, outVirtualPath, outGUID);
    }

    bool RegisterImportMaterial(void* userData, const std::filesystem::path& materialPath,
        const std::filesystem::path& modelDirectory,
        const Pitaya::Import::MaterialImportSource& source,
        Pitaya::Core::GUID& outGUID)
    {
        auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
        return assetHub && assetHub->RegisterGeneratedMaterial(materialPath, modelDirectory, source, outGUID);
    }
}

Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry*
Pitaya::Asset::AssetHub::FindTextureEntryCallback(void* userData, Pitaya::Core::GUID guid)
{
    auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
    if (!assetHub) { return nullptr; }

    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    assetHub->runtimeAssetManager.Textures().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* value)
        {
            entry = value;
        },
        [](Pitaya::Core::GUID) {});
    return entry;
}

Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry*
Pitaya::Asset::AssetHub::FindShaderEntryCallback(void* userData, Pitaya::Core::GUID guid)
{
    auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
    if (!assetHub) { return nullptr; }

    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    assetHub->runtimeAssetManager.Shaders().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* value)
        {
            entry = value;
        },
        [](Pitaya::Core::GUID) {});
    return entry;
}

Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry*
Pitaya::Asset::AssetHub::FindMeshEntryCallback(void* userData, Pitaya::Core::GUID guid)
{
    auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
    if (!assetHub) { return nullptr; }

    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    assetHub->runtimeAssetManager.Meshes().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* value)
        {
            entry = value;
        },
        [](Pitaya::Core::GUID) {});
    return entry;
}

Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry*
Pitaya::Asset::AssetHub::FindRenderTargetEntryCallback(void* userData, Pitaya::Core::GUID guid)
{
    auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
    if (!assetHub) { return nullptr; }

    Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* entry = nullptr;
    assetHub->runtimeAssetManager.RenderTargets().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* value)
        {
            entry = value;
        },
        [](Pitaya::Core::GUID) {});
    return entry;
}

Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry*
Pitaya::Asset::AssetHub::FindSkyBoxEntryCallback(void* userData, Pitaya::Core::GUID guid)
{
    auto* assetHub = static_cast<Pitaya::Asset::AssetHub*>(userData);
    if (!assetHub) { return nullptr; }

    Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* entry = nullptr;
    assetHub->runtimeAssetManager.SkyBoxes().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* value)
        {
            entry = value;
        },
        [](Pitaya::Core::GUID) {});
    return entry;
}

Pitaya::Import::ImportContext Pitaya::Asset::AssetHub::GetImportContext() noexcept
{
    return Pitaya::Import::ImportContext{
        this,
        &TransformImportPath,
        &ResolveImportGUID,
        &RegisterImportFile,
        &RegisterImportMaterial,
        Pitaya::Asset::Material::PBR
    };
}

void Pitaya::Asset::AssetHub::RegisterBuiltinAssets()
{
    builtinCatalog.Register<Pitaya::Asset::Texture>(Pitaya::Asset::Texture::White, "White");
    builtinCatalog.Register<Pitaya::Asset::Texture>(Pitaya::Asset::Texture::Black, "Black");
    builtinCatalog.Register<Pitaya::Asset::Texture>(Pitaya::Asset::Texture::FlatNormal, "FlatNormal");
    builtinCatalog.Register<Pitaya::Asset::Shader>(Pitaya::Asset::Shader::PBRStatic, "PBRStatic");
    builtinCatalog.Register<Pitaya::Asset::Shader>(Pitaya::Asset::Shader::PBRSkinned, "PBRSkinned");
    builtinCatalog.Register<Pitaya::Asset::Material>(Pitaya::Asset::Material::PBR, "PBR");
    builtinCatalog.Register<Pitaya::Asset::Mesh>(Pitaya::Asset::Mesh::Cube, "Cube");
    builtinCatalog.Register<Pitaya::Asset::Mesh>(Pitaya::Asset::Mesh::Panel, "Panel");
    builtinCatalog.Register<Pitaya::Asset::Mesh>(Pitaya::Asset::Mesh::Sphere, "Sphere");
    builtinCatalog.Register<Pitaya::Asset::SkyBox>(Pitaya::Asset::SkyBox::Default, "Default");
}

bool Pitaya::Asset::AssetHub::Initialize()
{
    if (!builtins.IsValid())
    {
        Pitaya::Log::Error("builtin asset storage allocation failed");
        return false;
    }
    builtins.White->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Texture>);
    builtins.Black->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Texture>);
    builtins.FlatNormal->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Texture>);
    builtins.PBRStaticShader->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Shader>);
    builtins.PBRSkinnedShader->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Shader>);
    builtins.PBRMaterial->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Material>);
    builtins.Cube->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Mesh>);
    builtins.Panel->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Mesh>);
    builtins.Sphere->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::Mesh>);
    builtins.DefaultSkyBox->SetOrphanReclaimFunction(&DestroyOrphanEntry<Pitaya::Asset::SkyBox>);
    RegisterBuiltinAssets();
    gpuAssetBridge.SetTextureCallbacks(this, &Pitaya::Asset::AssetHub::FindTextureEntryCallback);
    gpuAssetBridge.SetShaderCallbacks(this, &Pitaya::Asset::AssetHub::FindShaderEntryCallback);
    gpuAssetBridge.SetMeshCallbacks(this, &Pitaya::Asset::AssetHub::FindMeshEntryCallback);
    gpuAssetBridge.SetRenderTargetCallbacks(this, &Pitaya::Asset::AssetHub::FindRenderTargetEntryCallback);
    gpuAssetBridge.SetSkyBoxCallbacks(this, &Pitaya::Asset::AssetHub::FindSkyBoxEntryCallback);
    const std::filesystem::path engineRoot = Pitaya::Core::GetExecutableDirectory() / "resource";
    const std::filesystem::path projectRoot = Pitaya::Core::GetWorkspace() / "Asset" / "Resource";
    if (!virtualFileSystem.Initialize(engineRoot, projectRoot))
    {
        Pitaya::Log::Error("asset virtual file system could not be initialized");
        return false;
    }
    const std::filesystem::path registryFile = projectRoot / fileName;
    if (!database.Initialize(registryFile))
    {
        Pitaya::Log::Error("asset database could not be initialized, path:" + registryFile.string());
        return false;
    }
    importPipeline.SetContext(GetImportContext());

    //White Texture
    do
    {
        builtins.White->Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        builtins.White->GUID = Pitaya::Asset::Texture::White;
        builtins.White->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Color;
        result.isNearest = true;
        result.Data = { 255, 255, 255, 255 };	//纯白贴图
        result.GUID = Pitaya::Asset::Texture::White;
        builtins.White->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Textures().Emplace(Pitaya::Asset::Texture::White, builtins.White);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //Black Texture
    do
    {
        builtins.Black->Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        builtins.Black->GUID = Pitaya::Asset::Texture::Black;
        builtins.Black->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Color;
        result.isNearest = true;
        result.Data = { 0, 0, 0, 255 };
        result.GUID = Pitaya::Asset::Texture::Black;
        builtins.Black->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Textures().Emplace(Pitaya::Asset::Texture::Black, builtins.Black);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //FlatNormal Texture
    do
    {
        builtins.FlatNormal->Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        builtins.FlatNormal->GUID = Pitaya::Asset::Texture::FlatNormal;
        builtins.FlatNormal->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Linear;
        result.isNearest = true;
        result.Data = { 128, 128, 255, 255 };
        result.GUID = Pitaya::Asset::Texture::FlatNormal;
        builtins.FlatNormal->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Textures().Emplace(Pitaya::Asset::Texture::FlatNormal, builtins.FlatNormal);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //PBR Static Shader
    do
    {
        builtins.PBRStaticShader->Data.store(Pitaya::Core::New<Pitaya::Asset::Shader>(), std::memory_order_release);
        builtins.PBRStaticShader->GUID = Pitaya::Asset::Shader::PBRStatic;
        {
            auto* shader = builtins.PBRStaticShader->Data.load(std::memory_order_acquire);
            shader->ParamLayout.Slots =
            {
                { "BaseColorFactor", Pitaya::Asset::ParamType::Vector,  0, 0,  16 },
                { "EmissiveFactor",  Pitaya::Asset::ParamType::Vector,  1, 16, 16 },
                { "MetallicFactor",    Pitaya::Asset::ParamType::Float, 0, 32, 4 },
                { "RoughnessFactor",   Pitaya::Asset::ParamType::Float, 1, 36, 4 },
                { "OcclusionStrength", Pitaya::Asset::ParamType::Float, 2, 40, 4 },
                { "AlphaCutoff",       Pitaya::Asset::ParamType::Float, 3, 44, 4 },
                { "uAlbedoMap",    Pitaya::Asset::ParamType::Texture, 0, 48, 8 },
                { "uNormalMap",    Pitaya::Asset::ParamType::Texture, 1, 56, 8 },
                { "uMetallicMap",  Pitaya::Asset::ParamType::Texture, 2, 64, 8 },
                { "uRoughnessMap", Pitaya::Asset::ParamType::Texture, 3, 72, 8 },
                { "uAOMap",        Pitaya::Asset::ParamType::Texture, 4, 80, 8 },
                { "uEmissiveMap",  Pitaya::Asset::ParamType::Texture, 5, 88, 8 },
            };
            shader->ParamLayout.VectorCount = 2;
            shader->ParamLayout.FloatCount = 4;
            shader->ParamLayout.TextureCount = 6;
            shader->ParamLayout.TotalBytes = 96;
        }
        builtins.PBRStaticShader->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::ShaderImportResult result;
        {
            auto vsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_STATIC_VERTEX_SHADER);
            auto fsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_STATIC_FRAGMENT_SHADER);
            result.VertexSource = std::string(static_cast<const char*>(vsRc.data), vsRc.size);
            result.FragmentSource = std::string(static_cast<const char*>(fsRc.data), fsRc.size);
        }
        result.Type = Pitaya::GPU::Shader::VF;
        result.GUID = Pitaya::Asset::Shader::PBRStatic;
        builtins.PBRStaticShader->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Shaders().Emplace(Pitaya::Asset::Shader::PBRStatic, builtins.PBRStaticShader);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //PBR Skinned Shader
    do
    {
        builtins.PBRSkinnedShader->Data.store(Pitaya::Core::New<Pitaya::Asset::Shader>(), std::memory_order_release);
        builtins.PBRSkinnedShader->GUID = Pitaya::Asset::Shader::PBRSkinned;
        {
            auto* shader = builtins.PBRSkinnedShader->Data.load(std::memory_order_acquire);
            shader->ParamLayout.Slots =
            {
                { "BaseColorFactor", Pitaya::Asset::ParamType::Vector,  0, 0,  16 },
                { "EmissiveFactor",  Pitaya::Asset::ParamType::Vector,  1, 16, 16 },
                { "MetallicFactor",    Pitaya::Asset::ParamType::Float, 0, 32, 4 },
                { "RoughnessFactor",   Pitaya::Asset::ParamType::Float, 1, 36, 4 },
                { "OcclusionStrength", Pitaya::Asset::ParamType::Float, 2, 40, 4 },
                { "AlphaCutoff",       Pitaya::Asset::ParamType::Float, 3, 44, 4 },
                { "uAlbedoMap",    Pitaya::Asset::ParamType::Texture, 0, 48, 8 },
                { "uNormalMap",    Pitaya::Asset::ParamType::Texture, 1, 56, 8 },
                { "uMetallicMap",  Pitaya::Asset::ParamType::Texture, 2, 64, 8 },
                { "uRoughnessMap", Pitaya::Asset::ParamType::Texture, 3, 72, 8 },
                { "uAOMap",        Pitaya::Asset::ParamType::Texture, 4, 80, 8 },
                { "uEmissiveMap",  Pitaya::Asset::ParamType::Texture, 5, 88, 8 },
            };
            shader->ParamLayout.VectorCount = 2;
            shader->ParamLayout.FloatCount = 4;
            shader->ParamLayout.TextureCount = 6;
            shader->ParamLayout.TotalBytes = 96;
        }
        builtins.PBRSkinnedShader->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::ShaderImportResult result;
        {
            auto vsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_SKINNED_VERTEX_SHADER);
            auto fsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_SKINNED_FRAGMENT_SHADER);
            result.VertexSource = std::string(static_cast<const char*>(vsRc.data), vsRc.size);
            result.FragmentSource = std::string(static_cast<const char*>(fsRc.data), fsRc.size);
        }
        result.Type = Pitaya::GPU::Shader::VF;
        result.GUID = Pitaya::Asset::Shader::PBRSkinned;
        builtins.PBRSkinnedShader->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Shaders().Emplace(Pitaya::Asset::Shader::PBRSkinned, builtins.PBRSkinnedShader);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //PBR Material
    do
    {
        builtins.PBRMaterial->Data.store(Pitaya::Core::New<Pitaya::Asset::Material>(), std::memory_order_release);
        builtins.PBRMaterial->GUID = Pitaya::Asset::Material::PBR;
        builtins.PBRMaterial->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        auto* materialNativeData = builtins.PBRMaterial->Data.load(std::memory_order_acquire);
        materialNativeData->Shader = LoadAsset<Pitaya::Asset::Shader>(Pitaya::Asset::Shader::PBRStatic);
        materialNativeData->VectorParams.resize(materialNativeData->Shader->ParamLayout.VectorCount);
        materialNativeData->FloatParams.resize(materialNativeData->Shader->ParamLayout.FloatCount);
        materialNativeData->Textures.resize(materialNativeData->Shader->ParamLayout.TextureCount);
        materialNativeData->SetVec4("BaseColorFactor", glm::vec4(1.0f));
        materialNativeData->SetVec4("EmissiveFactor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        materialNativeData->SetFloat("MetallicFactor", 0.0f);
        materialNativeData->SetFloat("RoughnessFactor", 0.5f);
        materialNativeData->SetFloat("OcclusionStrength", 1.0f);
        materialNativeData->SetFloat("AlphaCutoff", 0.5f);
        materialNativeData->SetTexture("uAlbedoMap",    Pitaya::Asset::Texture::White);
        materialNativeData->SetTexture("uNormalMap",    Pitaya::Asset::Texture::FlatNormal);
        materialNativeData->SetTexture("uMetallicMap",  Pitaya::Asset::Texture::White);
        materialNativeData->SetTexture("uRoughnessMap", Pitaya::Asset::Texture::White);
        materialNativeData->SetTexture("uAOMap",        Pitaya::Asset::Texture::White);
        materialNativeData->SetTexture("uEmissiveMap",  Pitaya::Asset::Texture::Black);
        builtins.PBRMaterial->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        builtins.PBRMaterial->State.SetBits(Pitaya::Core::AssetState::GPULoaded);
        runtimeAssetManager.Materials().Emplace(Pitaya::Asset::Material::PBR, builtins.PBRMaterial);
    } while (false);

    //Cube Mesh
    do
    {
        builtins.Cube->Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        builtins.Cube->GUID = Pitaya::Asset::Mesh::Cube;
        builtins.Cube->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::StaticMeshImportResult result;
        result.GUID = Pitaya::Asset::Mesh::Cube;
        result.IsValid = true;
        std::vector<Pitaya::Import::StaticMeshVertex> vertices =
        {
            // 前面 (+Z)
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

            // 后面 (-Z)
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},

            // 左面 (-X)
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

            // 右面 (+X)
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},

            // 上面 (+Y)
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

            // 下面 (-Y)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}} };
        result.Vertices.resize(vertices.size() * 12);
        std::memcpy(result.Vertices.data(), vertices.data(), vertices.size() * sizeof(Pitaya::Import::StaticMeshVertex));
        for (uint32_t i = 0; i < 6; ++i)
        {
            uint32_t start = i * 4;
            result.Indices.push_back(start + 0);	// 三角形1
            result.Indices.push_back(start + 1);
            result.Indices.push_back(start + 2);
            result.Indices.push_back(start + 2);	// 三角形2
            result.Indices.push_back(start + 3);
            result.Indices.push_back(start + 0);
        }
        result.BoundingBox.Min = glm::vec3(-0.5f);
        result.BoundingBox.Max = glm::vec3(0.5f);
        Pitaya::Asset::Mesh::SubMesh sub;
        sub.BaseIndex = 0;
        sub.BaseVertex = 0;
        sub.IndexCount = 36;
        sub.MaterialIndex = 0;
        sub.BoundingBox = result.BoundingBox;
        result.SubMeshs.push_back(sub);
        result.VertexLayout = {
            { Pitaya::GPU::ShaderVariableType::Float3, 0 },
            { Pitaya::GPU::ShaderVariableType::Float3, 1 },
            { Pitaya::GPU::ShaderVariableType::Float2, 2 },
            { Pitaya::GPU::ShaderVariableType::Float4, 3 } };
        result.MaterialGUIDs.push_back(Pitaya::Asset::Material::PBR);
        builtins.Cube->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Meshes().Emplace(Pitaya::Asset::Mesh::Cube, builtins.Cube);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //Panel Mesh
    do
    {
        builtins.Panel->Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        builtins.Panel->GUID = Pitaya::Asset::Mesh::Panel;
        builtins.Panel->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::StaticMeshImportResult result;
        result.GUID = Pitaya::Asset::Mesh::Panel;
        result.IsValid = true;
        std::vector<Pitaya::Import::StaticMeshVertex> vertices =
        {
            // Position            | Normal          | UV          | Tangent (Handedness 1.0)
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}
        };
        result.Vertices.resize(vertices.size() * 12);
        memcpy(result.Vertices.data(), vertices.data(), vertices.size() * sizeof(Pitaya::Import::StaticMeshVertex));
        result.Indices = { 0, 1, 2, 2, 3, 0 };
        Pitaya::Asset::Mesh::SubMesh sub;
        sub.IndexCount = 6;
        sub.BaseIndex = 0;
        sub.BaseVertex = 0;
        sub.MaterialIndex = 0;
        sub.BoundingBox = { {-0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f} };
        result.SubMeshs.push_back(sub);
        result.VertexLayout = {
            { Pitaya::GPU::ShaderVariableType::Float3, 0 },
            { Pitaya::GPU::ShaderVariableType::Float3, 1 },
            { Pitaya::GPU::ShaderVariableType::Float2, 2 },
            { Pitaya::GPU::ShaderVariableType::Float4, 3 } };
        result.BoundingBox = sub.BoundingBox;
        result.MaterialGUIDs.push_back(Pitaya::Asset::Material::PBR);
        builtins.Panel->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Meshes().Emplace(Pitaya::Asset::Mesh::Panel, builtins.Panel);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //Sphere Mesh
    do
    {
        builtins.Sphere->Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        builtins.Sphere->GUID = Pitaya::Asset::Mesh::Sphere;
        builtins.Sphere->State.SetBits(Pitaya::Core::AssetState::CPULoading);

        Pitaya::Import::StaticMeshImportResult result;
        result.GUID = Pitaya::Asset::Mesh::Sphere;
        result.IsValid = true;

        // 算法生成 CCW 球体
        std::vector<Pitaya::Import::StaticMeshVertex> vertices;
        constexpr const float radius = 0.5f;           // 球体半径
        constexpr const uint32_t sectorCount = 36;     // 经线段数
        constexpr const uint32_t stackCount = 18;      // 纬线段数
        constexpr const float PI = 3.14159265359f;

        float sectorStep = 2.0f * PI / sectorCount;
        float stackStep = PI / stackCount;

        // 生成顶点 (Position, Normal, UV, Tangent)
        for (uint32_t i = 0; i <= stackCount; ++i)
        {
            // y纬度角：从顶部的 PI/2 逐渐降到底部的 -PI/2
            float stackAngle = PI / 2.0f - i * stackStep;
            float xz = radius * std::cos(stackAngle);
            float y = radius * std::sin(stackAngle);

            for (uint32_t j = 0; j <= sectorCount; ++j)
            {
                float sectorAngle = j * sectorStep;

                float x = xz * std::sin(sectorAngle);
                float z = xz * std::cos(sectorAngle);

                glm::vec3 position = { x, y, z };
                glm::vec3 normal = { x / radius, y / radius, z / radius };

                // UV：U 沿经度(0->1)， V 沿纬度(1->0) 兼容底层原点设定
                float u = (float)j / sectorCount;
                float v = 1.0f - (float)i / stackCount;
                glm::vec2 uv = { u, v };

                // 切线方程：向 U 递增的方向求导 (即沿着经线球体表面的切线)
                glm::vec3 tangentDir = { std::cos(sectorAngle), 0.0f, -std::sin(sectorAngle) };
                glm::vec4 tangent = { tangentDir.x, tangentDir.y, tangentDir.z, 1.0f };

                vertices.push_back({ position, normal, uv, tangent });
            }
        }

        // 构建满足逆时针(CCW)规范的顶点索引
        for (uint32_t i = 0; i < stackCount; ++i)
        {
            for (uint32_t j = 0; j < sectorCount; ++j)
            {
                uint32_t first = (i * (sectorCount + 1)) + j;
                uint32_t second = first + sectorCount + 1;

                // 顶部极点 此时第一圈退化成了三角形，不需要渲染完整的四边形
                if (i != 0)
                {
                    // 逆时针环绕 (Top-Left -> Bottom-Left -> Top-Right)
                    result.Indices.push_back(first);
                    result.Indices.push_back(second);
                    result.Indices.push_back(first + 1);
                }

                // 底部极点 
                if (i != (stackCount - 1))
                {
                    // 逆时针环绕 (Top-Right -> Bottom-Left -> Bottom-Right)
                    result.Indices.push_back(first + 1);
                    result.Indices.push_back(second);
                    result.Indices.push_back(second + 1);
                }
            }
        }
        // 像之前生成立方体一样，将顶点写入 Result 缓冲池
        result.Vertices.resize(vertices.size() * 12); // 注意这里用的是 12 floats! (3个位置 + 3个法线 + 2个UV + 4个切线)
        std::memcpy(result.Vertices.data(), vertices.data(), vertices.size() * sizeof(Pitaya::Import::StaticMeshVertex));

        // 强制定义包围盒大小
        result.BoundingBox.Min = glm::vec3(-radius);
        result.BoundingBox.Max = glm::vec3(radius);

        // 设置 SubMesh 参数
        Pitaya::Asset::Mesh::SubMesh sub;
        sub.BaseIndex = 0;
        sub.BaseVertex = 0;
        sub.IndexCount = static_cast<uint32_t>(result.Indices.size());
        sub.MaterialIndex = 0;
        sub.BoundingBox = result.BoundingBox;
        result.SubMeshs.push_back(sub);
        result.VertexLayout = {
            { Pitaya::GPU::ShaderVariableType::Float3, 0 },    // Pos
            { Pitaya::GPU::ShaderVariableType::Float3, 1 },    // Normal
            { Pitaya::GPU::ShaderVariableType::Float2, 2 },    // UV
            { Pitaya::GPU::ShaderVariableType::Float4, 3 } };     // Tangent
        result.MaterialGUIDs.push_back(Pitaya::Asset::Material::PBR);

        // 提交到引擎系统
        builtins.Sphere->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.Meshes().Emplace(Pitaya::Asset::Mesh::Sphere, builtins.Sphere);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    //Default SkyBox
    do
    {
        builtins.DefaultSkyBox->Data.store(Pitaya::Core::New<Pitaya::Asset::SkyBox>(), std::memory_order_release);
        builtins.DefaultSkyBox->GUID = Pitaya::Asset::SkyBox::Default;
        builtins.DefaultSkyBox->State.SetBits(Pitaya::Core::AssetState::CPULoading);
        auto rc = Pitaya::Core::LoadBuiltInRC(IDR_SKYBOX_HDR);
        Pitaya::Import::SkyBoxImportResult result;
        importPipeline.ImportSkyBox(Pitaya::Asset::SkyBox::Default, rc.data, rc.size, true, result);
        builtins.DefaultSkyBox->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        runtimeAssetManager.SkyBoxes().Emplace(Pitaya::Asset::SkyBox::Default, builtins.DefaultSkyBox);
        gpuAssetBridge.Enqueue(Pitaya::Asset::AssetOperate{ std::move(result) });
    } while (false);

    return true;
}
void Pitaya::Asset::AssetHub::Release()
{
    //从资源池移除内置资源
    runtimeAssetManager.Shaders().Erase(Pitaya::Asset::Shader::PBRStatic);
    runtimeAssetManager.Shaders().Erase(Pitaya::Asset::Shader::PBRSkinned);
    runtimeAssetManager.Textures().Erase(Pitaya::Asset::Texture::White);
    runtimeAssetManager.Textures().Erase(Pitaya::Asset::Texture::Black);
    runtimeAssetManager.Textures().Erase(Pitaya::Asset::Texture::FlatNormal);
    runtimeAssetManager.Materials().Erase(Pitaya::Asset::Material::PBR);
    runtimeAssetManager.Meshes().Erase(Pitaya::Asset::Mesh::Cube);
    runtimeAssetManager.Meshes().Erase(Pitaya::Asset::Mesh::Sphere);
    runtimeAssetManager.Meshes().Erase(Pitaya::Asset::Mesh::Panel);
    runtimeAssetManager.SkyBoxes().Erase(Pitaya::Asset::SkyBox::Default);

    // 先从 map 脱离所有 Entry，但暂时不删除 Entry。本阶段必须保证所有 Entry
    // 都还活着，因为 Material/Mesh 的析构会释放它们持有的 AssetRef。
    auto meshEntries = runtimeAssetManager.Meshes().ExtractAll();
    auto textureEntries = runtimeAssetManager.Textures().ExtractAll();
    auto shaderEntries = runtimeAssetManager.Shaders().ExtractAll();
    auto materialEntries = runtimeAssetManager.Materials().ExtractAll();
    auto renderTargetEntries = runtimeAssetManager.RenderTargets().ExtractAll();
    auto skyBoxEntries = runtimeAssetManager.SkyBoxes().ExtractAll();

    auto clearEntryData = [](auto& entries)
    {
        for (auto& [guid, entry] : entries)
        {
            (void)guid;
            if (!entry) { continue; }
            auto* data = entry->Data.exchange(nullptr, std::memory_order_acq_rel);
            Pitaya::Core::Delete(data);
            entry->State = Pitaya::Core::AssetState::Empty;
        }
    };

    // Entry 在这一阶段全部保留，所以跨资源 AssetRef 的析构顺序不会访问悬空指针。
    clearEntryData(meshEntries);
    clearEntryData(textureEntries);
    clearEntryData(shaderEntries);
    clearEntryData(materialEntries);
    clearEntryData(renderTargetEntries);
    clearEntryData(skyBoxEntries);

    auto clearBuiltinData = [](auto* entry)
    {
        if (!entry) { return; }
        auto* data = entry->Data.exchange(nullptr, std::memory_order_acq_rel);
        Pitaya::Core::Delete(data);
        entry->State = Pitaya::Core::AssetState::Empty;
    };

    clearBuiltinData(builtins.PBRStaticShader);
    clearBuiltinData(builtins.PBRSkinnedShader);
    clearBuiltinData(builtins.White);
    clearBuiltinData(builtins.Black);
    clearBuiltinData(builtins.FlatNormal);
    clearBuiltinData(builtins.PBRMaterial);
    clearBuiltinData(builtins.Cube);
    clearBuiltinData(builtins.Sphere);
    clearBuiltinData(builtins.Panel);
    clearBuiltinData(builtins.DefaultSkyBox);

    auto destroyBuiltinEntry = [](auto*& entry)
    {
        if (!entry) { return; }
        auto* retainedEntry = entry;
        entry = nullptr;
        if (retainedEntry->RefCount.HasRef())
        {
            Pitaya::Log::Warning("builtin asset entry retained after AssetHub shutdown, GUID:{}", retainedEntry->GUID);
            // 外部 AssetRef 仍然可能在引擎模块释放后析构。Entry 脱离 AssetHub
            // 后继续保留，Data 已清空，避免访问已经销毁的内嵌成员。
        }
        retainedEntry->MarkOrphaned();
    };

    destroyBuiltinEntry(builtins.PBRStaticShader);
    destroyBuiltinEntry(builtins.PBRSkinnedShader);
    destroyBuiltinEntry(builtins.White);
    destroyBuiltinEntry(builtins.Black);
    destroyBuiltinEntry(builtins.FlatNormal);
    destroyBuiltinEntry(builtins.PBRMaterial);
    destroyBuiltinEntry(builtins.Cube);
    destroyBuiltinEntry(builtins.Sphere);
    destroyBuiltinEntry(builtins.Panel);
    destroyBuiltinEntry(builtins.DefaultSkyBox);

    auto destroyEntries = [](auto& entries)
    {
        for (auto& [guid, entry] : entries)
        {
            if (!entry) { continue; }
            if (entry->RefCount.HasRef())
            {
                Pitaya::Log::Warning("asset entry retained after AssetHub shutdown, GUID:{}", guid);
                // 外部 AssetRef 仍可能在引擎模块释放后析构。保留 Entry 可以避免
                // AssetRef 析构时访问已释放的引用计数；Data 已经被置空。
            }
            entry->MarkOrphaned();
        }
        entries.clear();
    };

    destroyEntries(meshEntries);
    destroyEntries(textureEntries);
    destroyEntries(shaderEntries);
    destroyEntries(materialEntries);
    destroyEntries(renderTargetEntries);
    destroyEntries(skyBoxEntries);

    if (!database.Save())
    {
        Pitaya::Log::Warning("asset database could not be saved, path:"
            + (virtualFileSystem.GetRoot(Pitaya::Asset::VirtualFileSystem::MountId::Project) / fileName).string());
    }
}

void Pitaya::Asset::AssetHub::SyncAssetOperate(std::monostate&)
{
    Pitaya::Log::Error("unknown cpu operate result");
}
//TODO 资产模块 从该行向下 全部需要重构
// TODO 重构
bool Pitaya::Asset::AssetHub::TransformToVirtualPath(const std::filesystem::path& inputPath, const std::filesystem::path& basePath, std::filesystem::path& out_virtualpath) const
{
    if (virtualFileSystem.ToVirtualPath(inputPath, basePath, out_virtualpath)) { return true; }

    Pitaya::Log::Error("transform absolute to virtual path fail, absolute not engine or project, path:" + inputPath.string());
    return false;
}
bool Pitaya::Asset::AssetHub::RegisterExternalFile(const std::filesystem::path& inputPath, const std::filesystem::path& basePath, std::filesystem::path& out_virtualpath, Pitaya::Core::GUID& out_guid)
{
    if (!TransformToVirtualPath(inputPath, basePath, out_virtualpath))
    {
        Pitaya::Log::Error("register external file fail, transform fail, path:" + inputPath.string());
        return false;
    }

    //如果是引擎内置资源
    if (IsEngineAssetPath(out_virtualpath))
    {
        Pitaya::Log::Error("register external file fail, cant registe engine buildin asset, path:" + inputPath.string());
        return false;
    }

    //如果已经包含该路径
    if (GetAssetGUIDByPath(out_virtualpath, out_guid))
    {
        Pitaya::Log::Info("erternal asset has registerd, path:" + inputPath.string());
        return true;
    }

    out_guid = Pitaya::Core::GUID::New();
    if (!database.RegisterAssetPath(out_guid, out_virtualpath))
    {
        Pitaya::Log::Error("asset database could not register path:" + out_virtualpath.string());
        return false;
    }
    if (!database.Save())
    {
        Pitaya::Log::Warning("asset registry could not be saved, path:"
            + (virtualFileSystem.GetRoot(Pitaya::Asset::VirtualFileSystem::MountId::Project) / fileName).string());
    }
    return true;
}

bool Pitaya::Asset::AssetHub::RegisterGeneratedMaterial(const std::filesystem::path& materialPath,
    const std::filesystem::path& modelDirectory,
    const Pitaya::Import::MaterialImportSource& source,
    Pitaya::Core::GUID& out_guid)
{
    Pitaya::Asset::Material material;
    material.Shader = LoadAsset<Pitaya::Asset::Shader>(Pitaya::Asset::Shader::PBRStatic);
    if (!material.Shader)
    {
        Pitaya::Log::Error("generated material cannot resolve PBR shader, path:" + materialPath.string());
        return false;
    }

    material.VectorParams.resize(material.Shader->ParamLayout.VectorCount);
    material.FloatParams.resize(material.Shader->ParamLayout.FloatCount);
    material.Textures.resize(material.Shader->ParamLayout.TextureCount);

    for (const auto& property : source.FloatProperties)
    {
        material.SetFloat(property.Name, property.Value);
    }
    for (const auto& property : source.VectorProperties)
    {
        material.SetVec4(property.Name, property.Value);
    }
    for (const auto& property : source.TextureProperties)
    {
        const std::filesystem::path absoluteTexturePath =
            property.SourcePath.is_absolute() ? property.SourcePath : modelDirectory / property.SourcePath;
        std::filesystem::path virtualTexturePath;
        Pitaya::Core::GUID textureGUID = Pitaya::Asset::Texture::White;
        if (TransformToVirtualPath(absoluteTexturePath, modelDirectory, virtualTexturePath))
        {
            if (!GetAssetGUIDByPath(virtualTexturePath, textureGUID))
            {
                RegisterExternalFile(absoluteTexturePath, modelDirectory, virtualTexturePath, textureGUID);
            }
        }
        material.SetTexture(property.Name, textureGUID);
    }

    if (!material.SerializeToFile(materialPath))
    {
        Pitaya::Log::Error("generated material serialize fail, path:" + materialPath.string());
        return false;
    }

    std::filesystem::path virtualMaterialPath;
    if (!RegisterExternalFile(materialPath, modelDirectory, virtualMaterialPath, out_guid))
    {
        return false;
    }

    Pitaya::Asset::AssetRecord materialRecord;
    if (database.FindByGUID(out_guid, materialRecord))
    {
        materialRecord.Key.Type = Pitaya::Asset::AssetTypeId::Material;
        materialRecord.Origin = Pitaya::Asset::AssetOrigin::Generated;
        database.RegisterAsset(materialRecord);
        database.Save();
    }
    return true;
}

//Pitaya::Asset::AssetType Pitaya::Asset::AssetHub::GetAssetType(const std::filesystem::path& path) const
//{
//	if (!CheckIsVirtualPath(path))
//	{
//		Pitaya::Log::Error("get asset type fail! path is not virtual path, path:" + path.string());
//		return Pitaya::Asset::AssetType::Unknown;
//	}
//
//	std::filesystem::path resolvePath = GetResolvePath(path);
//	if (!CheckAssetValid(resolvePath))
//	{
//		Pitaya::Log::Error("get asset type fail! path invalid, path:" + resolvePath.string());
//		return Pitaya::Asset::AssetType::Unknown;
//	}
//
//	if (std::filesystem::is_regular_file(resolvePath))
//	{
//		std::string ext = resolvePath.extension().string();
//		Pitaya::Core::ToLower(ext);
//
//		//Texture
//		if (TextureExtensions.contains(ext) || ext == ".cubemap") { return Pitaya::Asset::AssetType::Texture; }
//
//		//Shader
//		if (ext == ".shader") { return Pitaya::Asset::AssetType::Shader; }
//
//		//Material
//		if (MaterialExtensions.contains(ext)) { return Pitaya::Asset::AssetType::Material; }
//
//		//Mesh
//		if (MeshExtensions.contains(ext)) { return Pitaya::Asset::AssetType::Mesh; }
//
//		//RenderTarget
//		if (RenderTargetExtensions.contains(ext)) { return Pitaya::Asset::AssetType::RenderTarget; }
//	}
//
//	Pitaya::Log::Error("unknwon path:" + path.string());
//	return Pitaya::Asset::AssetType::Unknown;
//}

bool Pitaya::Asset::AssetHub::CheckIsVirtualPath(const std::filesystem::path& path) const
{
    return virtualFileSystem.IsVirtualPath(path);
}
bool Pitaya::Asset::AssetHub::CheckAssetValid(const std::filesystem::path& path) const
{
    //TODO 后续可升级为检测文件是否符合加载条件，如果不符合不会加载
    return !path.empty() && std::filesystem::exists(path);
}

bool Pitaya::Asset::AssetHub::CheckIsValidTexture2DFile(const std::filesystem::path& file) const
{
    if (!std::filesystem::is_regular_file(file)) { return false; }

    std::string ext = file.extension().string();
    Pitaya::Core::ToLower(ext);
    if (TextureExtensions.find(ext) == TextureExtensions.end())
    {
        Pitaya::Log::Info("texture2D extension" + ext + ", path:" + file.string());
        return false;
    }
    return true;
}
bool Pitaya::Asset::AssetHub::CheckIsValidTextureCubeMapFolder(const std::filesystem::path& folder, std::vector<std::filesystem::path>& out) const
{
    if (!std::filesystem::is_directory(folder)) { return false; }

    out.clear();
    out.resize(static_cast<size_t>(Pitaya::GPU::TextureCubemapFace::Unknown));

    uint32_t validCount = 0;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            if (!entry.is_regular_file()) { continue; }

            std::filesystem::path path = entry.path();
            std::string ext = path.extension().string();;
            Pitaya::Core::ToLower(ext);
            if (ext == ".cubemap") { continue; }
            if (TextureExtensions.find(ext) == TextureExtensions.end())
            {
                Pitaya::Log::Info("texturecubemap extension" + ext + ", path:" + path.string());
                continue;
            }

            std::string name = path.stem().string();
            Pitaya::GPU::TextureCubemapFace face = Pitaya::GPU::ToEnum<Pitaya::GPU::TextureCubemapFace>(name);
            if (face != Pitaya::GPU::TextureCubemapFace::Unknown && out[static_cast<size_t>(face)].empty())
            {
                validCount++;
                out[static_cast<size_t>(face)] = std::move(path);
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        Pitaya::Log::Error("traverse directory fail in CheckIsValidTextureCubeMapFolder! path:" + folder.string() + ", error:" + e.what());
    }
    catch (...)
    {
        Pitaya::Log::Error("traverse directory fail in CheckIsValidTextureCubeMapFolder! path:" + folder.string() + ", unknwon error");
    }
    return validCount == 6;
}
bool Pitaya::Asset::AssetHub::CheckIsValidShaderFloder(const std::filesystem::path& folder, std::unordered_map<Pitaya::GPU::ShaderType, std::filesystem::path>& out) const
{
    if (!std::filesystem::is_directory(folder)) { return false; }

    out.clear();
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            if (!entry.is_regular_file()) { continue; }

            std::string ext = entry.path().extension().string();
            Pitaya::Core::ToLower(ext);
            if (ext == ".shader") { continue; }

            auto extIt = ShaderExtensions.find(ext);
            if (extIt == ShaderExtensions.end())
            {
                Pitaya::Log::Info("unsupported shader file extension: " + ext + " in folder: " + folder.string());
                continue;
            }

            Pitaya::GPU::ShaderType shaderType = extIt->second;
            auto shaderIt = out.find(shaderType);
            if (shaderIt != out.end())
            {
                Pitaya::Log::Error(
                    "Duplicate shader type in folder: " + folder.string() + "\n"
                    "  New path: " + entry.path().string() + "\n"
                    "  Old path: " + shaderIt->second.string());
            }
            out[shaderType] = entry.path();
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        Pitaya::Log::Error("traverse directory fail in CheckIsValidShaderFloder! path:" + folder.string() + ", error:" + e.what());
    }
    catch (...)
    {
        Pitaya::Log::Error("traverse directory fail in CheckIsValidTextureCubeMapFolder! path:" + folder.string() + ", unknwon error");
    }
    return out.find(Pitaya::GPU::ShaderType::Vertex) != out.end() &&
        out.find(Pitaya::GPU::ShaderType::Fragment) != out.end(); //至少需要顶点着色器和片元着色器
}
bool Pitaya::Asset::AssetHub::CheckIsValidMaterialFile(const std::filesystem::path& file) const
{
    if (!std::filesystem::is_regular_file(file)) { return false; }

    std::string ext = file.extension().string();
    Pitaya::Core::ToLower(ext);
    return MaterialExtensions.contains(ext);
}
bool Pitaya::Asset::AssetHub::CheckIsValidRenderTargetFile(const std::filesystem::path& file) const
{
    if (!std::filesystem::is_regular_file(file)) { return false; }

    std::string ext = file.extension().string();
    Pitaya::Core::ToLower(ext);
    return RenderTargetExtensions.contains(ext);
}
bool Pitaya::Asset::AssetHub::CheckIsValidMeshFile(const std::filesystem::path& path) const
{
    if (!std::filesystem::is_regular_file(path)) 
    { 
        Pitaya::Log::Error("mesh asset not file, path:" + path.string());
        return false; 
    }

    std::string ext = path.extension().string();
    Pitaya::Core::ToLower(ext);
    if (MeshExtensions.find(ext) == MeshExtensions.end())
    {
        Pitaya::Log::Info("error mesh file extension" + ext);
        return false;
    }
    return true;
}

void Pitaya::Asset::AssetHub::LoadTextureAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, uint64_t generation)
{
    //文件为Texture2D
    if (CheckIsValidTexture2DFile(path))
    {
        Pitaya::Import::Texture2DImportResult cpuOpResult_Inner;
        if (LoadTexture2DAsset(guid, path, generation, cpuOpResult_Inner))
        {
            Pitaya::Log::Info(path.string() + "Load Success");
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error(path.string() + "Load Fail");
        }
        return;
    }

    //文件为TextureCubemap
    std::vector<std::filesystem::path> outFaceFiles;
    if (CheckIsValidTextureCubeMapFolder(path.parent_path(), outFaceFiles))
    {
        Pitaya::Import::TextureCubemapImportResult cpuOpResult_Inner;
        if (LoadTextureCubemapAsset(guid, path, outFaceFiles, generation, cpuOpResult_Inner))
        {
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error(path.string() + "Load Fail");
        }
        return;
    }

    //文件格式异常 标记CPU加载失败
    std::string log;
    runtimeAssetManager.Textures().FindOperateKV(guid,
        [&log, &path, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = _guid.ToString() + "Texture AssetEntry is empty path:" + path.string();
                return;
            }

            if (_entry->IsLoadGeneration(generation)) { _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed); }
        },
        [&log, &path](Pitaya::Core::GUID _guid)
        {
            log = _guid.ToString() + "Texture AssetEntry not found" + path.string();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    Pitaya::Log::Error("unknow texture file , path: " + path.string());
}
bool Pitaya::Asset::AssetHub::LoadTexture2DAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, uint64_t generation, Pitaya::Import::Texture2DImportResult& result)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Textures().FindOperateKV(guid,
        [&path, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "Texture AssetEntry not found" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "Texture asset marked as Unload, abort CPU load: " + path.string();
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "Texture AssetEntry not found" + _guid.ToString();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportTexture2D(guid, path, true, true, result))
    {
        Pitaya::Log::Error("texture load failed: " + path.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("Texture asset marked as Unload, abort CPU load: " + path.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}
bool Pitaya::Asset::AssetHub::LoadTextureCubemapAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, const std::vector<std::filesystem::path>& paths, uint64_t generation, Pitaya::Import::TextureCubemapImportResult& result)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Textures().FindOperateKV(guid,
        [&file, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "texture cubemap asset entry not found" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "texture cubemap asset marked as unload, abort CPU load: " + file.string();
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "texture cubemap asset entry not found" + _guid.ToString();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportTextureCubemap(guid, file, paths, true, true, result))
    {
        Pitaya::Log::Error("texture cubemap load failed: " + file.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("texture cubemap asset marked as Unload, abort CPU load: " + file.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}

void Pitaya::Asset::AssetHub::LoadShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, uint64_t generation)
{
    std::filesystem::path folder = file.parent_path();
    std::unordered_map<Pitaya::GPU::ShaderType, std::filesystem::path> out_paths;
    if (!CheckIsValidShaderFloder(folder, out_paths))
    {
        Pitaya::Log::Error(guid.ToString() + " shader asset is invalid path: " + folder.string());
        return;
    }

    std::filesystem::path vertexPath;
    std::filesystem::path fragmentPath;
    std::filesystem::path geometryPath;

    Pitaya::Core::StateFlags<Pitaya::GPU::ShaderType> state = Pitaya::GPU::ShaderType::Empty;
    auto iterator = out_paths.begin();
    while (iterator != out_paths.end())
    {
        switch (iterator->first)
        {
            case Pitaya::GPU::ShaderType::Vertex:   vertexPath = iterator->second;   break;
            case Pitaya::GPU::ShaderType::Fragment: fragmentPath = iterator->second; break;
            case Pitaya::GPU::ShaderType::Geometry: geometryPath = iterator->second; break;

            case Pitaya::GPU::ShaderType::Empty: Pitaya::Log::Error("empty state mark!"); break;
            default:							 Pitaya::Log::Error("empty state mark!"); break;
        }

        state.SetBits(iterator->first);
        iterator++;
    }

    if (state == Pitaya::GPU::Shader::VF)
    {
        Pitaya::Import::ShaderImportResult cpuOpResult_Inner;
        if (LoadVFShaderAsset(guid, folder, vertexPath, fragmentPath, generation, cpuOpResult_Inner))
        {
            Pitaya::Log::Info(folder.string() + " load success, in shader VF");
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error(folder.string() + " load fail");
        }
        return;
    }

    if (state == Pitaya::GPU::Shader::VFG)
    {
        Pitaya::Import::ShaderImportResult cpuOpResult_Inner;
        if (LoadVFGShaderAsset(guid, folder, vertexPath, fragmentPath, geometryPath, generation, cpuOpResult_Inner))
        {
            Pitaya::Log::Info(folder.string() + " load success");
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error(folder.string() + " load fail");
        }
        return;
    }

    //异常状态Shader 标记CPU加载失败
    std::string log;
    runtimeAssetManager.Shaders().FindOperateKV(guid,
        [&log, &folder, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = _guid.ToString() + "Shader AssetEntry is empty path:" + folder.string();
                return;
            }

            if (_entry->IsLoadGeneration(generation)) { _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed); }
        },
        [&log, &folder](Pitaya::Core::GUID _guid)
        {
            log = _guid.ToString() + "Shader AssetEntry not found" + folder.string();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    Pitaya::Log::Error("Unknow shader file, path: " + folder.string());
}
bool Pitaya::Asset::AssetHub::LoadVFShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, uint64_t generation, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner)
{
    std::string log;
    std::string strPath = folder.string();

    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Shaders().FindOperateKV(guid,
        [strPath, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "shader AssetEntry not found" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "shader asset marked as Unload, abort CPU load: " + strPath;
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "Shader AssetEntry not found" + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportShader(guid, folder, vertexPath, fragmentPath, cpuOpResult_Inner))
    {
        Pitaya::Log::Error("shader load failed: " + strPath);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("shader asset marked as Unload, abort CPU load: " + strPath);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}
bool Pitaya::Asset::AssetHub::LoadVFGShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, const std::filesystem::path& geometry, uint64_t generation, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner)
{
    std::string log;
    std::string strPath = folder.string();

    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Shaders().FindOperateKV(guid,
        [strPath, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "shader assetEntry not found" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "shader asset marked as Unload, abort CPU load: " + strPath;
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "Shader AssetEntry not found" + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportShader(guid, folder, vertexPath, fragmentPath, geometry, cpuOpResult_Inner))
    {
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("Shader asset marked as Unload, abort CPU load: " + strPath);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}

void Pitaya::Asset::AssetHub::LoadMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, uint64_t generation)
{
    if (!CheckIsValidMeshFile(file))
    {
        Pitaya::Log::Error(guid.ToString() + " mesh path error, path:" + file.string());
        return;
    }

    Pitaya::Import::MeshPreloadResult preloadResult;
    if (!importPipeline.PreloadMesh(guid, file, preloadResult))
    {
        Pitaya::Log::Error("GUID:" + guid.ToString() + " preload fail, path:" + file.string());
        return;
    }

    if (preloadResult.Type == Pitaya::GPU::MeshType::Static)
    {
        Pitaya::Import::StaticMeshImportResult cpuOpResult_Inner;
        if (LoadStaticMeshAsset(guid, file, preloadResult, generation, cpuOpResult_Inner))
        {	
            Pitaya::Log::Info("static mesh asset load success in cpu");
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error("static mesh load fail in cpu");
        }
        return;
    }

    if (preloadResult.Type == Pitaya::GPU::MeshType::SkinnedMesh)
    {
        Pitaya::Import::SkinnedMeshImportResult cpuOpResult_Inner;
        if (LoadSkinnedMeshAsset(guid, file, preloadResult, generation, cpuOpResult_Inner))
        {
            Pitaya::Log::Info("skinned mesh asset load success in cpu");
            EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);
        }
        else
        {
            Pitaya::Log::Error("skinned mesh load fail in cpu");
        }
        return;
    }

    Pitaya::Log::Error("load emsh fail,unknwon mesh type, GUID:" + guid.ToString() + " ,path:" + file.string());
}
bool Pitaya::Asset::AssetHub::LoadStaticMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preloadResult, uint64_t generation, Pitaya::Import::StaticMeshImportResult& out)
{
    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Meshes().FindOperateKV(guid,
        [path, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "mesh asset entry is empty, GUID:" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "mesh asset marked as Unload, abort CPU load: " + path;
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "mesh asset entry not found" + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportStaticMesh(guid, file, preloadResult, out))
    {
        Pitaya::Log::Error("static mesh load fail, GUID:" + guid.ToString() + ", path:" + path);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("mesh asset marked as Unload, abort CPU load: " + path);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}
bool Pitaya::Asset::AssetHub::LoadSkinnedMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, const Pitaya::Import::MeshPreloadResult& detectResult, uint64_t generation, Pitaya::Import::SkinnedMeshImportResult& out)
{
    std::string log;
    std::string stringPath = path.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Meshes().FindOperateKV(guid,
        [stringPath, &log, &entry, generation](Pitaya::Core::GUID assetGuid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* assetEntry)
        {
            if (!assetEntry)
            {
                log = "mesh asset entry is empty, GUID:" + assetGuid.ToString();
                return;
            }

            if (!assetEntry->IsLoadGeneration(generation)) { return; }

            if (assetEntry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "skinned mesh asset marked as Unload, abort CPU load: " + stringPath;
                assetEntry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            assetEntry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = assetEntry;
        },
        [&log](Pitaya::Core::GUID assetGuid)
        {
            log = "mesh asset entry not found, GUID:" + assetGuid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return false; }

    if (!importPipeline.ImportSkinnedMesh(guid, path, detectResult, out))
    {
        Pitaya::Log::Error("skinned mesh load fail, GUID:" + guid.ToString() + ", path:" + stringPath);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("skinned mesh asset marked as Unload, abort CPU load: " + stringPath);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return false;
    }

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
    return true;
}

void Pitaya::Asset::AssetHub::LoadMaterialAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, uint64_t generation)
{
    if (!CheckIsValidMaterialFile(file))
    {
        Pitaya::Log::Error("material invalid, path:" + file.string());
        return;
    }

    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry* entry = nullptr;
    runtimeAssetManager.Materials().FindOperateKV(guid,
        [path, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "material asset entry is empty" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "material asset marked as unload, abort CPU load: " + path;
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "material asset entry not found" + _guid.ToString();
        });
    if (!log.empty()){ Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return; }

    auto* material = entry->Data.load(std::memory_order_acquire);
    if (!material)
    {
        Pitaya::Log::Error(guid.ToString() + " asset entry data is empty,path:" + path);
        entry->FailCpuLoad();
        return;
    }

    if (!material->DeserializeFromFile(file))
    {
        Pitaya::Log::Error("material deserialize fail, path:" + file.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        entry->FailCpuLoad();
        return;
    }

    //Material无GPU资源 直接标记为GPULoaded即可
    if (entry->IsLoadGeneration(generation))
    {
        entry->State.SetBits(Pitaya::Core::AssetState::GPULoaded);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    }
    Pitaya::Log::Info("material load success, path: " + path + " , GUID: " + guid.ToString());
}

void Pitaya::Asset::AssetHub::LoadRenderTarget(Pitaya::Core::GUID guid, const std::filesystem::path& file, uint64_t generation)
{
    if (!CheckIsValidRenderTargetFile(file))
    {
        Pitaya::Log::Error("rendertarget invalid, path:" + file.string());
        return;
    }

    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* entry = nullptr;
    runtimeAssetManager.RenderTargets().FindOperateKV(guid,
        [path, &log, &entry, generation](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "rendertarget asset entry is empty" + _guid.ToString();
                return;
            }

            if (!_entry->IsLoadGeneration(generation)) { return; }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "rendertarget asset marked as unload, abort CPU load: " + path;
                _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "rendertarget asset entry not found" + _guid.ToString();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry || !entry->IsLoadGeneration(generation)) { return; }

    auto* rendertarget = entry->Data.load(std::memory_order_acquire);
    if (!rendertarget)
    {
        Pitaya::Log::Error(guid.ToString() + "rendertarget asset entry data is empty,path:" + path);
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return;
    }

    if (!rendertarget->DeserializeFromFile(file))
    {
        Pitaya::Log::Error("rendertarget deserialize fail, path:" + file.string());
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return;
    }

    if (!entry->IsLoadGeneration(generation) || entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading); }
        return;
    }

    Pitaya::Log::Info("rendertarget CPU laod success, GUID:" + guid.ToString());
    Pitaya::Import::RenderTargetImportResult cpuOpResult_Inner;
    cpuOpResult_Inner.GUID = guid;
    EnqueueAssetOperate(std::move(cpuOpResult_Inner), generation);

    if (entry->IsLoadGeneration(generation)) { entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading); }
}
void Pitaya::Asset::AssetHub::LoadSkyBoxAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, uint64_t generation)
{
    Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* entry = nullptr;
    runtimeAssetManager.SkyBoxes().FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* value) { entry = value; },
        [](Pitaya::Core::GUID) {});
    if (!entry || !entry->IsLoadGeneration(generation) || entry->IsUnloadRequested())
    {
        if (entry) { entry->FailCpuLoad(); }
        return;
    }

    Pitaya::Import::SkyBoxImportResult result;
    if (importPipeline.ImportSkyBox(guid, path, true, result))
    {
        if (!entry->IsLoadGeneration(generation) || entry->IsUnloadRequested())
        {
            entry->FailCpuLoad();
            return;
        }
        entry->CompleteCpuLoad();
        EnqueueAssetOperate(std::move(result), generation);
    }
    else
    {
        Pitaya::Log::Error(path.string() + " SkyBox Load Fail");
        entry->FailCpuLoad();
    }
}

//ENDTODO
