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
}

bool Pitaya::Asset::AssetHub::Initialize()
{
    engineRoot = Pitaya::Core::GetExecutableDirectory() / "resource";
    projectRoot = Pitaya::Core::GetWorkspace() / "Asset" / "Resource";
    const std::filesystem::path registryFile = projectRoot / fileName;
    if (std::filesystem::exists(registryFile))
    {
        registry.DeserializeFromFile(registryFile);
    }

    //White Texture
    do
    {
        buildIn.White.Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        buildIn.White.GUID = Pitaya::Asset::Texture::White;
        buildIn.White.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Color;
        result.isNearest = true;
        result.Data = { 255, 255, 255, 255 };	//纯白贴图
        result.GUID = Pitaya::Asset::Texture::White;
        buildIn.White.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        textures.Emplace(Pitaya::Asset::Texture::White, &buildIn.White);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //Black Texture
    do
    {
        buildIn.Black.Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        buildIn.Black.GUID = Pitaya::Asset::Texture::Black;
        buildIn.Black.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Color;
        result.isNearest = true;
        result.Data = { 0, 0, 0, 255 };
        result.GUID = Pitaya::Asset::Texture::Black;
        buildIn.Black.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        textures.Emplace(Pitaya::Asset::Texture::Black, &buildIn.Black);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //FlatNormal Texture
    do
    {
        buildIn.FlatNormal.Data.store(Pitaya::Core::New<Pitaya::Asset::Texture>(), std::memory_order_release);
        buildIn.FlatNormal.GUID = Pitaya::Asset::Texture::FlatNormal;
        buildIn.FlatNormal.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::Texture2DImportResult result;
        result.Width = 1;
        result.Height = 1;
        result.Channels = 4;
        result.IsGenerateMipmap = false;
        result.Usage = Pitaya::Asset::TextureUsage::Linear;
        result.isNearest = true;
        result.Data = { 128, 128, 255, 255 };
        result.GUID = Pitaya::Asset::Texture::FlatNormal;
        buildIn.FlatNormal.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        textures.Emplace(Pitaya::Asset::Texture::FlatNormal, &buildIn.FlatNormal);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //PBR Static Shader
    do
    {
        buildIn.PBRStaticShader.Data.store(Pitaya::Core::New<Pitaya::Asset::Shader>(), std::memory_order_release);
        buildIn.PBRStaticShader.GUID = Pitaya::Asset::Shader::PBRStatic;
        {
            auto* shader = buildIn.PBRStaticShader.Data.load(std::memory_order_acquire);
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
        buildIn.PBRStaticShader.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::ShaderImportResult result;
        {
            auto vsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_STATIC_VERTEX_SHADER);
            auto fsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_STATIC_FRAGMENT_SHADER);
            result.VertexSource = std::string(static_cast<const char*>(vsRc.data), vsRc.size);
            result.FragmentSource = std::string(static_cast<const char*>(fsRc.data), fsRc.size);
        }
        result.Type = Pitaya::GPU::Shader::VF;
        result.GUID = Pitaya::Asset::Shader::PBRStatic;
        buildIn.PBRStaticShader.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        shaders.Emplace(Pitaya::Asset::Shader::PBRStatic, &buildIn.PBRStaticShader);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //PBR Skinned Shader
    do
    {
        buildIn.PBRSkinnedShader.Data.store(Pitaya::Core::New<Pitaya::Asset::Shader>(), std::memory_order_release);
        buildIn.PBRSkinnedShader.GUID = Pitaya::Asset::Shader::PBRSkinned;
        {
            auto* shader = buildIn.PBRSkinnedShader.Data.load(std::memory_order_acquire);
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
        buildIn.PBRSkinnedShader.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        Pitaya::Import::ShaderImportResult result;
        {
            auto vsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_SKINNED_VERTEX_SHADER);
            auto fsRc = Pitaya::Core::LoadBuiltInRC(IDR_PBR_SKINNED_FRAGMENT_SHADER);
            result.VertexSource = std::string(static_cast<const char*>(vsRc.data), vsRc.size);
            result.FragmentSource = std::string(static_cast<const char*>(fsRc.data), fsRc.size);
        }
        result.Type = Pitaya::GPU::Shader::VF;
        result.GUID = Pitaya::Asset::Shader::PBRSkinned;
        buildIn.PBRSkinnedShader.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        shaders.Emplace(Pitaya::Asset::Shader::PBRSkinned, &buildIn.PBRSkinnedShader);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //PBR Material
    do
    {
        buildIn.PBRMaterial.Data.store(Pitaya::Core::New<Pitaya::Asset::Material>(), std::memory_order_release);
        buildIn.PBRMaterial.GUID = Pitaya::Asset::Material::PBR;
        buildIn.PBRMaterial.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        auto* materialNativeData = buildIn.PBRMaterial.Data.load(std::memory_order_acquire);
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
        buildIn.PBRMaterial.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        buildIn.PBRMaterial.State.SetBits(Pitaya::Core::AssetState::GPULoaded);
        materials.Emplace(Pitaya::Asset::Material::PBR, &buildIn.PBRMaterial);
    } while (false);

    //Cube Mesh
    do
    {
        buildIn.Cube.Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        buildIn.Cube.GUID = Pitaya::Asset::Mesh::Cube;
        buildIn.Cube.State.SetBits(Pitaya::Core::AssetState::CPULoading);
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
        buildIn.Cube.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        meshes.Emplace(Pitaya::Asset::Mesh::Cube, &buildIn.Cube);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //Panel Mesh
    do
    {
        buildIn.Panel.Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        buildIn.Panel.GUID = Pitaya::Asset::Mesh::Panel;
        buildIn.Panel.State.SetBits(Pitaya::Core::AssetState::CPULoading);
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
        buildIn.Panel.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        meshes.Emplace(Pitaya::Asset::Mesh::Panel, &buildIn.Panel);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //Sphere Mesh
    do
    {
        buildIn.Sphere.Data.store(Pitaya::Core::New<Pitaya::Asset::Mesh>(), std::memory_order_release);
        buildIn.Sphere.GUID = Pitaya::Asset::Mesh::Sphere;
        buildIn.Sphere.State.SetBits(Pitaya::Core::AssetState::CPULoading);

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
        buildIn.Sphere.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        meshes.Emplace(Pitaya::Asset::Mesh::Sphere, &buildIn.Sphere);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    //Default SkyBox
    do
    {
        buildIn.DefaultSkyBox.Data.store(Pitaya::Core::New<Pitaya::Asset::SkyBox>(), std::memory_order_release);
        buildIn.DefaultSkyBox.GUID = Pitaya::Asset::SkyBox::Default;
        buildIn.DefaultSkyBox.State.SetBits(Pitaya::Core::AssetState::CPULoading);
        auto rc = Pitaya::Core::LoadBuiltInRC(IDR_SKYBOX_HDR);
        Pitaya::Import::SkyBoxImportResult result;
        Pitaya::Import::Import(Pitaya::Asset::SkyBox::Default, rc.data, rc.size, true, result);
        buildIn.DefaultSkyBox.State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
        skyboxes.Emplace(Pitaya::Asset::SkyBox::Default, &buildIn.DefaultSkyBox);
        cacheAssetOperateQueue.push({ result });
    } while (false);

    return true;
}
void Pitaya::Asset::AssetHub::Release()
{
    // TODO 重构
    //从资源池移除内置资源
    shaders.Erase(Pitaya::Asset::Shader::PBRStatic);
    shaders.Erase(Pitaya::Asset::Shader::PBRSkinned);
    textures.Erase(Pitaya::Asset::Texture::White);
    textures.Erase(Pitaya::Asset::Texture::Black);
    textures.Erase(Pitaya::Asset::Texture::FlatNormal);
    materials.Erase(Pitaya::Asset::Material::PBR);
    meshes.Erase(Pitaya::Asset::Mesh::Cube);
    meshes.Erase(Pitaya::Asset::Mesh::Sphere);
    meshes.Erase(Pitaya::Asset::Mesh::Panel);
    skyboxes.Erase(Pitaya::Asset::SkyBox::Default);

    //清理资源池资源
    meshes.ForEachCheckErase(
        [](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                //清除Entry内部data数据
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);

                //清除Entry
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });
    textures.ForEachCheckErase(
        [](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                //清除Entry内部data数据
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);

                //清除Entry
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });
    shaders.ForEachCheckErase(
        [](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                //清除Entry内部data数据
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);

                //清除Entry
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });
    materials.ForEachCheckErase(
        [](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });
    rendertargets.ForEachCheckErase(
        [](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                //清除Entry内部data数据
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);

                //清除Entry
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });
    skyboxes.ForEachCheckErase(
        [](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry*& _entry)
        {
            if (_entry)
            {
                Pitaya::Core::Delete(_entry->Data.load(std::memory_order_acquire));
                _entry->Data.store(nullptr, std::memory_order_release);
                Pitaya::Core::Delete(_entry);
                _entry = nullptr;
            }
            return true;
        });

    //清除内置资源
    Pitaya::Core::Delete(buildIn.PBRStaticShader.Data.load(std::memory_order_acquire)); buildIn.PBRStaticShader.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.PBRSkinnedShader.Data.load(std::memory_order_acquire)); buildIn.PBRSkinnedShader.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.White.Data.load(std::memory_order_acquire)); buildIn.White.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.Black.Data.load(std::memory_order_acquire)); buildIn.Black.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.FlatNormal.Data.load(std::memory_order_acquire)); buildIn.FlatNormal.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.PBRMaterial.Data.load(std::memory_order_acquire)); buildIn.PBRMaterial.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.Cube.Data.load(std::memory_order_acquire)); buildIn.Cube.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.Sphere.Data.load(std::memory_order_acquire)); buildIn.Sphere.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.Panel.Data.load(std::memory_order_acquire)); buildIn.Panel.Data.store(nullptr, std::memory_order_release);
    Pitaya::Core::Delete(buildIn.DefaultSkyBox.Data.load(std::memory_order_acquire)); buildIn.DefaultSkyBox.Data.store(nullptr, std::memory_order_release);

    registry.SerializeToFile(projectRoot / fileName);
}

void Pitaya::Asset::AssetHub::SyncAssetOperate(std::monostate&)
{
    Pitaya::Log::Error("unknown cpu operate result");
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::Texture2DImportResult& cpuOpResult_Inner)
{
    char log[1024] = {};
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    textures.FindOperateKV(cpuOpResult_Inner.GUID,
        [&](Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry) { std::format_to_n(log, sizeof(log), "Texture AssetEntry Is Empty when GPU Process CPU Texture2D Load Result GUID: {}", _guid); return; }
            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload)) { std::format_to_n(log, sizeof(log), "Texture asset marked as Unload, abort GPU load GUID: {}", _guid); return; }
            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&](Core::GUID _guid)
        {
            std::format_to_n(log, sizeof(log), "Texture AssetEntry not found when GPU Process CPU Texture2D Load Result GUID: {}", _guid);
        });
    if (log[0] != '\0') { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    auto* texture2D = entry->Data.load(std::memory_order_acquire);
    if (!texture2D)
    {
        Pitaya::Log::Error("texture2D asset entry data is Empty! GUID: {}", cpuOpResult_Inner.GUID);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    const Pitaya::GPU::PixelFormat texturePixelFormat = Pitaya::Asset::TextureUsageToPixelFormat(cpuOpResult_Inner.Usage);
    auto texture2DHandle = Pitaya::GPU::CreateTexture2D(cpuOpResult_Inner.Data.data(), cpuOpResult_Inner.Width, cpuOpResult_Inner.Height, texturePixelFormat, cpuOpResult_Inner.IsGenerateMipmap, cpuOpResult_Inner.isNearest);
    if (!texture2DHandle)
    {
        Pitaya::Log::Error("texture2D asset create fail! GUID: {}", cpuOpResult_Inner.GUID);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("Texture2D asset marked as Unload, abort GPU load GUID:{}", cpuOpResult_Inner.GUID);
        if (!Pitaya::GPU::DestroyTexture2D(texture2DHandle)) { Pitaya::Log::Error("destroy texture2D asset gail GUID: {}", cpuOpResult_Inner.GUID); }
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    texture2D->Texture2DHandle = texture2DHandle;
    texture2D->Type = Pitaya::GPU::TextureType::Texture2D;
    texture2D->Usage = cpuOpResult_Inner.Usage;
    Pitaya::Log::Info("successfully created GPU resources for texture2D GUID: {}", cpuOpResult_Inner.GUID);
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::TextureCubemapImportResult& cpuOpResult_Inner)
{
    char log[1024]{};
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    textures.FindOperateKV(cpuOpResult_Inner.GUID,
        [&](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry) { std::format_to_n(log, sizeof(log), "texture asset entry is empty when GPU process CPU texture cubemap load result GUID: {}", _guid); return; }
            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload)) { std::format_to_n(log, sizeof(log), "texture asset marked as Unload, abort GPU load GUID:{}", _guid); return;}
            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&](Pitaya::Core::GUID _guid)
        {
            std::format_to_n(log, sizeof(log), "texture cubemap asset entry not found when GPU process CPU texture cubemap load result GUID: {}", _guid);
        });
    if (log[0] != '\0') { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    auto* textureCubemap = entry->Data.load(std::memory_order_acquire);
    if (!textureCubemap)
    {
        Pitaya::Log::Error("texture cubemap asset entry data is Empty! GUID: {}", cpuOpResult_Inner.GUID);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    const void* datas[6] = {};
    for (uint32_t i = 0; i < 6; i++)
    {
        datas[i] = cpuOpResult_Inner.Data[i].data();
    }
    const Pitaya::GPU::PixelFormat cubemapPixelFormat = Pitaya::Asset::TextureUsageToPixelFormat(cpuOpResult_Inner.Usage);
    auto textureCubemapHandle = Pitaya::GPU::CreateTextureCubemap(datas, cpuOpResult_Inner.Width, cpuOpResult_Inner.Height, cubemapPixelFormat, cpuOpResult_Inner.IsGenerateMipmap, cpuOpResult_Inner.isNearest);
    if (!textureCubemapHandle)
    {
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("texture cubemap asset marked as Unload, abort GPU load GUID:{}", cpuOpResult_Inner.GUID);
        if (!Pitaya::GPU::DestroyTextureCubemap(textureCubemapHandle))
        {
            Pitaya::Log::Error("destroy texture cubemap asset fail, GUID:{}", cpuOpResult_Inner.GUID);
        }
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    textureCubemap->TextureCubemapHandle = textureCubemapHandle;
    textureCubemap->Type = Pitaya::GPU::TextureType::TextureCubemap;
    textureCubemap->Usage = cpuOpResult_Inner.Usage;
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::ShaderImportResult& cpuOpResult_Inner)
{
    char log[1024] = {};
    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    shaders.FindOperateKV(cpuOpResult_Inner.GUID,
        [&](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry) { std::format_to_n(log, sizeof(log), "Shader AssetEntry Is Empty when GPU Process CPU Shader Load Result GUID: {}", _guid); return; }
            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload)) { std::format_to_n(log, sizeof(log), "Shader asset marked as Unload, abort GPU load GUID: {}", _guid); return; }
            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&](Pitaya::Core::GUID _guid)
        {
            std::format_to_n(log, sizeof(log), "Shader AssetEntry not found when GPU Process CPU Shader Load Result GUID: {}", _guid);
        });
    if (log[0] != '\0') { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    auto* shader = entry->Data.load(std::memory_order_acquire);
    if (!shader)
    {
        Pitaya::Log::Error("Shader asset entry data is Empty! GUID: {}", cpuOpResult_Inner.GUID);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    auto shaderHandle = Pitaya::Core::SlotMap<Pitaya::GPU::Shader>::Handle::Invalid;
    if (cpuOpResult_Inner.Type == Pitaya::GPU::Shader::VF)
    {
        if (!cpuOpResult_Inner.VertexSource.empty() && !cpuOpResult_Inner.FragmentSource.empty())
        {
            shaderHandle = Pitaya::GPU::CreateShader(cpuOpResult_Inner.VertexSource.c_str(), cpuOpResult_Inner.VertexSource.size(), cpuOpResult_Inner.FragmentSource.c_str(), cpuOpResult_Inner.FragmentSource.size());
        }
        else
        {
            Pitaya::Log::Error("cpu load shader result is empty GUID: {}", cpuOpResult_Inner.GUID);
        }
    }
    else if (cpuOpResult_Inner.Type == Pitaya::GPU::Shader::VFG)
    {
        if (!cpuOpResult_Inner.VertexSource.empty() && !cpuOpResult_Inner.FragmentSource.empty() && !cpuOpResult_Inner.GeometrySource.empty())
        {
            shaderHandle = Pitaya::GPU::CreateShader(cpuOpResult_Inner.VertexSource.c_str(), cpuOpResult_Inner.VertexSource.size(), cpuOpResult_Inner.FragmentSource.c_str(), cpuOpResult_Inner.FragmentSource.size(), cpuOpResult_Inner.GeometrySource.c_str(), cpuOpResult_Inner.GeometrySource.size());
        }
        else
        {
            Pitaya::Log::Error("cpu load shader result is empty GUID: {}", cpuOpResult_Inner.GUID);
        }
    }
    else
    {
        Pitaya::Log::Error("erro shader type GUID: {}", cpuOpResult_Inner.GUID);
    }

    //无效句柄未加载GPU数据
    if (!shaderHandle)
    {
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        Pitaya::Log::Error("cpu load shader result is empty GUID: {}", cpuOpResult_Inner.GUID);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        Pitaya::Log::Error("Shader asset marked as Unload, abort GPU load GUID: {}", cpuOpResult_Inner.GUID);
        if (!Pitaya::GPU::DestroyShader(shaderHandle))
        {
            Pitaya::Log::Error("Shader asset destroy fali GUID:{}", cpuOpResult_Inner.GUID);
        }
        return;
    }

    Pitaya::Log::Info("shader gpu load success, GUID: {}", cpuOpResult_Inner.GUID);
    entry->Data.load(std::memory_order_acquire)->ShaderHandle = shaderHandle;
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::StaticMeshImportResult& cpuOpResult_Inner)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    meshes.FindOperateKV(cpuOpResult_Inner.GUID,
        [&log, &entry, &cpuOpResult_Inner](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "mesh asset entry is empty when GPU process CPU mesh load result GUID: " + _guid.ToString();
                return;
            }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "mesh asset marked as Unload, abort GPU load: " + _guid.ToString();
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "mesh asset entry not found when GPU process CPU mesh load result GUID: " + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    if (!cpuOpResult_Inner.IsValid || cpuOpResult_Inner.Vertices.empty() || cpuOpResult_Inner.Indices.empty())
    {
        Pitaya::Log::Error("Invalid CPU mesh data for GUID: " + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("mesh asset marked as Unload, abort GPU load" + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    auto* mesh = entry->Data.load(std::memory_order_acquire);
    if (!mesh)
    {
        Pitaya::Log::Error("Failed to get mesh entry data GUID: " + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    auto vaoHandle = Pitaya::GPU::CreateVertexArray();
    auto vboHandle = Pitaya::GPU::CreateVertexBuffer(cpuOpResult_Inner.Vertices.data(), static_cast<uint32_t>(cpuOpResult_Inner.Vertices.size() * sizeof(float)));
    auto iboHandle = Pitaya::GPU::CreateIndexBuffer(cpuOpResult_Inner.Indices.data(), static_cast<uint32_t>(cpuOpResult_Inner.Indices.size()));
    if (!Pitaya::GPU::LinkVertexArray(vaoHandle, vboHandle, iboHandle, cpuOpResult_Inner.VertexLayout))
    {
        Pitaya::Log::Error("failed to link VAO GUID: " + cpuOpResult_Inner.GUID.ToString());
        Pitaya::GPU::DestroyVertexArray(vaoHandle);
        Pitaya::GPU::DestroyVertexBuffer(vboHandle);
        Pitaya::GPU::DestroyIndexBuffer(iboHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("StaticMesh marked as Unload during upload, cleaning up GPU resources: " + cpuOpResult_Inner.GUID.ToString());
        Pitaya::GPU::DestroyVertexArray(vaoHandle);
        Pitaya::GPU::DestroyVertexBuffer(vboHandle);
        Pitaya::GPU::DestroyIndexBuffer(iboHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    mesh->VertexArrayHandle = vaoHandle;
    mesh->BoundingBox = cpuOpResult_Inner.BoundingBox;
    mesh->SubMeshs = std::move(cpuOpResult_Inner.SubMeshs);
    mesh->Type = Pitaya::GPU::MeshType::Static;
    for (const auto& materialGUID : cpuOpResult_Inner.MaterialGUIDs)
    {
        mesh->Materials.emplace_back(Pitaya::Asset::LoadAsset<Pitaya::Asset::Material>(materialGUID));
    }

    Pitaya::Log::Info("Successfully created GPU resources for static mesh GUID: " + cpuOpResult_Inner.GUID.ToString());
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::SkinnedMeshImportResult& cpuOpResult_Inner)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    meshes.FindOperateKV(cpuOpResult_Inner.GUID,
        [&log, &entry, &cpuOpResult_Inner](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "mesh asset entry is empty when GPU process CPU mesh load result GUID: " + _guid.ToString();
                return;
            }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "mesh asset marked as Unload, abort GPU load: " + _guid.ToString();
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "mesh asset entry not found when GPU process CPU mesh load result GUID: " + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    if (!cpuOpResult_Inner.IsValid || cpuOpResult_Inner.Vertices.empty() || cpuOpResult_Inner.Indices.empty())
    {
        Pitaya::Log::Error("Invalid CPU skinned mesh data for GUID: " + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("skinned mesh asset marked as Unload, abort GPU load" + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    auto* mesh = entry->Data.load(std::memory_order_acquire);
    if (!mesh)
    {
        Pitaya::Log::Error("Failed to get skinned mesh entry data GUID: " + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    auto vaoHandle = Pitaya::GPU::CreateVertexArray();
    auto vboHandle = Pitaya::GPU::CreateVertexBuffer(cpuOpResult_Inner.Vertices.data(), static_cast<uint32_t>(cpuOpResult_Inner.Vertices.size() * sizeof(float)));
    auto iboHandle = Pitaya::GPU::CreateIndexBuffer(cpuOpResult_Inner.Indices.data(), static_cast<uint32_t>(cpuOpResult_Inner.Indices.size()));
    if (!Pitaya::GPU::LinkVertexArray(vaoHandle, vboHandle, iboHandle, cpuOpResult_Inner.VertexLayout))
    {
        Pitaya::Log::Error("failed to link VAO GUID: " + cpuOpResult_Inner.GUID.ToString());
        Pitaya::GPU::DestroyVertexArray(vaoHandle);
        Pitaya::GPU::DestroyVertexBuffer(vboHandle);
        Pitaya::GPU::DestroyIndexBuffer(iboHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("SkinnedMesh marked as Unload during upload, cleaning up GPU resources: " + cpuOpResult_Inner.GUID.ToString());
        Pitaya::GPU::DestroyVertexArray(vaoHandle);
        Pitaya::GPU::DestroyVertexBuffer(vboHandle);
        Pitaya::GPU::DestroyIndexBuffer(iboHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    mesh->VertexArrayHandle = vaoHandle;
    mesh->SubMeshs = std::move(cpuOpResult_Inner.SubMeshs);

    mesh->BoneInverseMatrices = std::move(cpuOpResult_Inner.BoneInverseMatrices);
    mesh->BoneCount = cpuOpResult_Inner.BoneCount;
    mesh->BoundingBox = cpuOpResult_Inner.BoundingBox;
    for (const auto& materialGUID : cpuOpResult_Inner.MaterialGUIDs)
    {
        mesh->Materials.emplace_back(Pitaya::Asset::LoadAsset<Pitaya::Asset::Material>(materialGUID));
    }
    mesh->Type = Pitaya::GPU::MeshType::SkinnedMesh;

    Pitaya::Log::Info("successfully created GPU resources for skinned mesh GUID: " + cpuOpResult_Inner.GUID.ToString());
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::RenderTargetImportResult& cpuOpResult_Inner)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* entry = nullptr;
    rendertargets.FindOperateKV(cpuOpResult_Inner.GUID,
        [&log, &entry, &cpuOpResult_Inner](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "rendertarget asset entry is empty when GPU process CPU mesh load result GUID: " + _guid.ToString();
                return;
            }

            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload))
            {
                log = "rendertarget asset marked as Unload, abort GPU load: " + _guid.ToString();
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&log](Pitaya::Core::GUID _guid)
        {
            log = "rendertarget asset entry not found when GPU process CPU mesh load result GUID: " + _guid.ToString();
        });

    if (!log.empty()) { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    auto* rendertarget = entry->Data.load(std::memory_order_acquire);
    if (!rendertarget)
    {
        Pitaya::Log::Error("rendertarget asset entry data is Empty!" + cpuOpResult_Inner.GUID.ToString());
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    //Scene
    rendertarget->SceneFrameBufferHandle = Pitaya::GPU::CreateFrameBuffer(rendertarget->SceneFrameBufferSpecification);
    //PingPong
    rendertarget->PingPongFrameBufferHandles[0] = Pitaya::GPU::CreateFrameBuffer(rendertarget->PingPongFrameBufferSpecification);
    rendertarget->PingPongFrameBufferHandles[1] = Pitaya::GPU::CreateFrameBuffer(rendertarget->PingPongFrameBufferSpecification);
    //Final
    rendertarget->FinalFrameBufferHandle = Pitaya::GPU::CreateFrameBuffer(rendertarget->FinalFrameBufferSpecification);

    if (!rendertarget->SceneFrameBufferHandle || !rendertarget->PingPongFrameBufferHandles[0] || 
        !rendertarget->PingPongFrameBufferHandles[1] || !rendertarget->FinalFrameBufferHandle)
    {
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->SceneFrameBufferHandle);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->PingPongFrameBufferHandles[0]);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->PingPongFrameBufferHandles[1]);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->FinalFrameBufferHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("rendertarget asset marked as Unload, abort GPU load" + cpuOpResult_Inner.GUID.ToString());
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->SceneFrameBufferHandle);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->PingPongFrameBufferHandles[0]);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->PingPongFrameBufferHandles[1]);
        Pitaya::GPU::DestroyFrameBuffer(rendertarget->FinalFrameBufferHandle);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading);
        return;
    }

    Pitaya::Log::Info("successfully created GPU resources for framebuffer GUID: " + cpuOpResult_Inner.GUID.ToString());
    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Asset::Texture2DUnloadRequire& cpuOpResult_Inner)
{
    Pitaya::GPU::DestroyTexture2D(cpuOpResult_Inner.Texture2DHandle);
}
void Pitaya::Asset::AssetHub::SyncAssetOperate(Pitaya::Import::SkyBoxImportResult& cpuOpResult_Inner)
{
	char log[1024] = {};
    Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* entry = nullptr;
    skyboxes.FindOperateKV(cpuOpResult_Inner.GUID,
        [&](Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* _entry)
        {
            if (!_entry) { std::format_to_n(log, sizeof(log), "skybox assetentry Is Empty GUID: {}", _guid); return; }
            if (_entry->State.HasBits(Pitaya::Core::AssetState::Unload)) { std::format_to_n(log, sizeof(log), "skybox marked as unload GUID: {}", _guid); return; }
            _entry->State.SetBits(Pitaya::Core::AssetState::GPULoading);
            entry = _entry;
        },
        [&](Core::GUID _guid) { std::format_to_n(log, sizeof(log), "skybox assetentry not found GUID: {}", _guid); });
    if (log[0] != '\0') { Pitaya::Log::Error(log); }
    if (!entry) { return; }

    auto* skyBox = entry->Data.load(std::memory_order_acquire);
    if (!skyBox) { Pitaya::Log::Error("skybox asset entry data is Empty! GUID: {}", cpuOpResult_Inner.GUID); entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); return; }

    auto equirectHandle = Pitaya::GPU::CreateTexture2D(cpuOpResult_Inner.Data.data(),
        cpuOpResult_Inner.Width, cpuOpResult_Inner.Height, Pitaya::GPU::PixelFormat::RGBA16F, false, false);
    auto envHandle = Pitaya::GPU::CreateEmptyTextureCubemap(512, 10, Pitaya::GPU::PixelFormat::RGBA16F);
    auto irradianceHandle = Pitaya::GPU::CreateEmptyTextureCubemap(32, 1, Pitaya::GPU::PixelFormat::RGBA16F);
    auto prefilteredHandle = Pitaya::GPU::CreateEmptyTextureCubemap(256, 6, Pitaya::GPU::PixelFormat::RGBA16F);
    if (!equirectHandle || !envHandle || !irradianceHandle || !prefilteredHandle) { entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); return; }
    skyBox->EquirectHandle = equirectHandle;
    skyBox->EnvCubemapHandle = envHandle;
    skyBox->IrradianceHandle = irradianceHandle;
    skyBox->PrefilteredHandle = prefilteredHandle;

    if (!Pitaya::Render::BakeIBL({ equirectHandle, envHandle, irradianceHandle, prefilteredHandle }))
    {
        Pitaya::Log::Error("skybox asset entry data bake IBL fail! GUID: {}", cpuOpResult_Inner.GUID);
        entry->State.ModifyBits(Pitaya::Core::AssetState::GPUFailed, Pitaya::Core::AssetState::GPULoading); 
        return;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::GPULoaded, Pitaya::Core::AssetState::GPULoading);
}

//TODO 资产模块 从该行向下 全部需要重构
// TODO 重构
bool Pitaya::Asset::AssetHub::TransformToVirtualPath(const std::filesystem::path& inputPath, const std::filesystem::path& basePath, std::filesystem::path& out_virtualpath) const
{
    std::filesystem::path absolutePath = inputPath.is_absolute() ? inputPath : basePath / inputPath;
    try
    {
        absolutePath = std::filesystem::canonical(absolutePath);
    }
    catch (...)
    {
        try
        {
            absolutePath = std::filesystem::weakly_canonical(absolutePath);
        }
        catch (...)
        {
            absolutePath = std::filesystem::absolute(absolutePath);
        }
    }

    //如果是引擎内置资源
    if (Pitaya::Core::IsSubPath(absolutePath, engineRoot))
    {
        std::filesystem::path relative = std::filesystem::relative(absolutePath, engineRoot);
        out_virtualpath = "engine:/" + relative.generic_string();
        return true;
    }

    //如果是项目资源
    if (Pitaya::Core::IsSubPath(absolutePath, projectRoot))
    {
        std::filesystem::path relative = std::filesystem::relative(absolutePath, projectRoot);
        out_virtualpath = "project:/" + relative.generic_string();
        return true;
    }

    Pitaya::Log::Error("transform absolute to virtual path fail, absolute not engine or project, path:" + absolutePath.string());
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
    if (IsBuildInAsset(out_virtualpath))
    {
        Pitaya::Log::Error("register external file fail, cant registe engine buildin asset, path:" + inputPath.string());
        return false;
    }

    //如果已经包含该路径
    bool hasRegisted = false;
    registry.Map.FindOperateKV(out_virtualpath,
        [&hasRegisted, &out_guid](const std::filesystem::path& _path, Pitaya::Core::GUID _guid)
        {
            hasRegisted = true;
            out_guid = _guid;
        },
        [](const std::filesystem::path& _path)
        {

        });
    if (hasRegisted)
    {
        Pitaya::Log::Info("erternal asset has registerd, path:" + inputPath.string());
        return true;
    }

    out_guid = Pitaya::Core::GUID::New();
    registry.Map.Insert(out_guid, out_virtualpath);
    if (!registry.SerializeToFile(projectRoot / fileName))
    {
        Pitaya::Log::Warning("asset registry could not be saved, path:" + (projectRoot / fileName).string());
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
    std::string pathstr = path.string();
    return pathstr.starts_with("engine:/") || pathstr.starts_with("project:/");
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

void Pitaya::Asset::AssetHub::LoadTextureAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path)
{
    //文件为Texture2D
    if (CheckIsValidTexture2DFile(path))
    {
        Pitaya::Import::Texture2DImportResult cpuOpResult_Inner;
        if (LoadTexture2DAsset(guid, path, cpuOpResult_Inner))
        {
            Pitaya::Log::Info(path.string() + "Load Success");
            assetOperateQueue.Push({ cpuOpResult_Inner });
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
        if (LoadTextureCubemapAsset(guid, path, outFaceFiles, cpuOpResult_Inner))
        {
            Pitaya::Asset::AssetOperate cpuOpResult_Out;
            cpuOpResult_Out.Data = cpuOpResult_Inner;
            assetOperateQueue.Push(cpuOpResult_Out);
        }
        else
        {
            Pitaya::Log::Error(path.string() + "Load Fail");
        }
        return;
    }

    //文件格式异常 标记CPU加载失败
    std::string log;
    textures.FindOperateKV(guid,
        [&log, &path](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = _guid.ToString() + "Texture AssetEntry is empty path:" + path.string();
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
        },
        [&log, &path](Pitaya::Core::GUID _guid)
        {
            log = _guid.ToString() + "Texture AssetEntry not found" + path.string();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    Pitaya::Log::Error("unknow texture file , path: " + path.string());
}
bool Pitaya::Asset::AssetHub::LoadTexture2DAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, Pitaya::Import::Texture2DImportResult& result)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    textures.FindOperateKV(guid,
        [&path, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "Texture AssetEntry not found" + _guid.ToString();
                return;
            }

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
    if (!entry) { return false; }

    if (!Pitaya::Import::Import(guid, path, true, true, result))
    {
        Pitaya::Log::Error("texture load failed: " + path.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("Texture asset marked as Unload, abort CPU load: " + path.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    return true;
}
bool Pitaya::Asset::AssetHub::LoadTextureCubemapAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file,const std::vector<std::filesystem::path>& paths, Pitaya::Import::TextureCubemapImportResult& result)
{
    std::string log;
    Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* entry = nullptr;
    textures.FindOperateKV(guid,
        [&file, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "texture cubemap asset entry not found" + _guid.ToString();
                return;
            }

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
    if (!entry) { return false; }

    if (!Pitaya::Import::Import(guid, file, paths,true, true, result))
    {
        Pitaya::Log::Error("texture cubemap load failed: " + file.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("texture cubemap asset marked as Unload, abort CPU load: " + file.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    return true;
}

void Pitaya::Asset::AssetHub::LoadShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file)
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
        if (LoadVFShaderAsset(guid, folder, vertexPath, fragmentPath, cpuOpResult_Inner))
        {
            Pitaya::Log::Info(folder.string() + " load success, in shader VF");
            assetOperateQueue.Push({ cpuOpResult_Inner });
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
        if (LoadVFGShaderAsset(guid, folder, vertexPath, fragmentPath, geometryPath,cpuOpResult_Inner))
        {
            Pitaya::Log::Info(folder.string() + " load success");
            Pitaya::Asset::AssetOperate cpuOpResult_Out;
            cpuOpResult_Out.Data = cpuOpResult_Inner;
            assetOperateQueue.Push(cpuOpResult_Out);
        }
        else
        {
            Pitaya::Log::Error(folder.string() + " load fail");
        }
        return;
    }

    //异常状态Shader 标记CPU加载失败
    std::string log;
    shaders.FindOperateKV(guid,
        [&log, &folder](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = _guid.ToString() + "Shader AssetEntry is empty path:" + folder.string();
                return;
            }

            _entry->State.SetBits(Pitaya::Core::AssetState::CPUFailed);
        },
        [&log, &folder](Pitaya::Core::GUID _guid)
        {
            log = _guid.ToString() + "Shader AssetEntry not found" + folder.string();
        });
    if (!log.empty()) { Pitaya::Log::Error(log); }
    Pitaya::Log::Error("Unknow shader file, path: " + folder.string());
}
bool Pitaya::Asset::AssetHub::LoadVFShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner)
{
    std::string log;
    std::string strPath = folder.string();

    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    shaders.FindOperateKV(guid,
        [strPath, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "shader AssetEntry not found" + _guid.ToString();
                return;
            }

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
    if (!entry) { return false; }

    if (!Pitaya::Import::Import(guid, folder, vertexPath, fragmentPath, cpuOpResult_Inner))
    {
        Pitaya::Log::Error("shader load failed: " + strPath);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("shader asset marked as Unload, abort CPU load: " + strPath);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    return true;
}
bool Pitaya::Asset::AssetHub::LoadVFGShaderAsset(Pitaya::Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath, const std::filesystem::path& geometry, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner)
{
    std::string log;
    std::string strPath = folder.string();

    Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* entry = nullptr;
    shaders.FindOperateKV(guid,
        [strPath, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "shader assetEntry not found" + _guid.ToString();
                return;
            }

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
    if (!entry) { return false; }

    if (!Pitaya::Import::Import(guid, folder, vertexPath, fragmentPath, geometry, cpuOpResult_Inner))
    {
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("Shader asset marked as Unload, abort CPU load: " + strPath);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    return true;
}

void Pitaya::Asset::AssetHub::LoadMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file)
{
    if (!CheckIsValidMeshFile(file))
    {
        Pitaya::Log::Error(guid.ToString() + " mesh path error, path:" + file.string());
        return;
    }

    Pitaya::Import::MeshPreloadResult preloadResult;
    if (!Pitaya::Import::Import(guid, file, preloadResult))
    {
        Pitaya::Log::Error("GUID:" + guid.ToString() + " preload fail, path:" + file.string());
        return;
    }

    if (preloadResult.Type == Pitaya::GPU::MeshType::Static)
    {
        Pitaya::Import::StaticMeshImportResult cpuOpResult_Inner;
        if (LoadStaticMeshAsset(guid, file, preloadResult, cpuOpResult_Inner))
        {	
            Pitaya::Log::Info("static mesh asset load success in cpu");
            Pitaya::Asset::AssetOperate cpuOpResult_Out;
            cpuOpResult_Out.Data = cpuOpResult_Inner;
            assetOperateQueue.Push(cpuOpResult_Out);
        }
        else
        {
            Pitaya::Log::Error("static mesh load fail in cpu");
        }
        return;
    }

    if (preloadResult.Type == Pitaya::GPU::MeshType::SkinnedMesh)
    {
        //TODO 加载SkinedMesh
        return;
    }

    Pitaya::Log::Error("load emsh fail,unknwon mesh type, GUID:" + guid.ToString() + " ,path:" + file.string());
}
bool Pitaya::Asset::AssetHub::LoadStaticMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preloadResult, Pitaya::Import::StaticMeshImportResult& out)
{
    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* entry = nullptr;
    meshes.FindOperateKV(guid,
        [path, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "mesh asset entry is empty, GUID:" + _guid.ToString();
                return;
            }

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
    if (!entry) { return false; }

    if (!Pitaya::Import::Import(guid, file, preloadResult, out))
    {
        Pitaya::Log::Error("static mesh load fail, GUID:" + guid.ToString() + ", path:" + path);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    if (entry->State.HasBits(Pitaya::Core::AssetState::Unload))
    {
        Pitaya::Log::Error("mesh asset marked as Unload, abort CPU load: " + path);
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return false;
    }

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    return true;
}
bool Pitaya::Asset::AssetHub::LoadSkinnedMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path, const Pitaya::Import::MeshPreloadResult& detectResult,Pitaya::Import::SkinnedMeshImportResult& out)
{
    //TODO 加载骨骼动画
    Pitaya::Log::Debug("current engine version cant load skinned mesh asset, TAT... GUID:" +
        guid.ToString() + ".path:" + path.string());
    return false;
}

void Pitaya::Asset::AssetHub::LoadMaterialAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file)
{
    if (!CheckIsValidMaterialFile(file))
    {
        Pitaya::Log::Error("material invalid, path:" + file.string());
        return;
    }

    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry* entry = nullptr;
    materials.FindOperateKV(guid,
        [path,&log,&entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "material asset entry is empty" + _guid.ToString();
                return;
            }

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
    if (!entry) { return; }

    auto* material = entry->Data.load(std::memory_order_acquire);
    if (!material)
    {
        Pitaya::Log::Error(guid.ToString() + " asset entry data is empty,path:" + path);
        return;
    }

    if (!material->DeserializeFromFile(file))
    {
        Pitaya::Log::Error("material deserialize fail, path:" + file.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return;
    }

    //Material无GPU资源 直接标记为GPULoaded即可
    entry->State.SetBits(Pitaya::Core::AssetState::GPULoaded);
    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
    Pitaya::Log::Info("material load success, path: " + path + " , GUID: " + guid.ToString());
}

void Pitaya::Asset::AssetHub::LoadRenderTarget(Pitaya::Core::GUID guid, const std::filesystem::path& file)
{
    if (!CheckIsValidRenderTargetFile(file))
    {
        Pitaya::Log::Error("rendertarget invalid, path:" + file.string());
        return;
    }

    std::string log;
    std::string path = file.string();
    Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* entry = nullptr;
    rendertargets.FindOperateKV(guid,
        [path, &log, &entry](Pitaya::Core::GUID _guid, Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry* _entry)
        {
            if (!_entry)
            {
                log = "rendertarget asset entry is empty" + _guid.ToString();
                return;
            }

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
    if (!entry) { return; }

    auto* rendertarget = entry->Data.load(std::memory_order_acquire);
    if (!rendertarget)
    {
        Pitaya::Log::Error(guid.ToString() + "rendertarget asset entry data is empty,path:" + path);
        return;
    }

    if (!rendertarget->DeserializeFromFile(file))
    {
        Pitaya::Log::Error("rendertarget deserialize fail, path:" + file.string());
        entry->State.ModifyBits(Pitaya::Core::AssetState::CPUFailed, Pitaya::Core::AssetState::CPULoading);
        return;
    }

    Pitaya::Log::Info("rendertarget CPU laod success, GUID:" + guid.ToString());
    Pitaya::Import::RenderTargetImportResult cpuOpResult_Inner;
    cpuOpResult_Inner.GUID = guid;
    Pitaya::Asset::AssetOperate cpuOpResult_Out;
    cpuOpResult_Out.Data = cpuOpResult_Inner;
    assetOperateQueue.Push(cpuOpResult_Out);

    entry->State.ModifyBits(Pitaya::Core::AssetState::CPULoaded, Pitaya::Core::AssetState::CPULoading);
}
void Pitaya::Asset::AssetHub::LoadSkyBoxAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path)
{
    Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* entry = nullptr;
    skyboxes.FindOperateKV(guid,
        [&entry](Pitaya::Core::GUID, Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry* value) { entry = value; },
        [](Pitaya::Core::GUID) {});
    if (!entry || entry->IsUnloadRequested())
    {
        if (entry) { entry->FailCpuLoad(); }
        return;
    }

    Pitaya::Import::SkyBoxImportResult result;
    if (Pitaya::Import::Import(guid, path, true, result))
    {
        entry->CompleteCpuLoad();
        assetOperateQueue.Push({ result });
    }
    else
    {
        Pitaya::Log::Error(path.string() + " SkyBox Load Fail");
        entry->FailCpuLoad();
    }
}

void Pitaya::Asset::AssetHub::AssetRegistry::Serialize(Pitaya::Serialize::SerializeContext& context) const
{
    for (const auto& [guid, path] : Map.GetAllKeyValuePairs())
    {
        auto& entry = context.GetSubContext(guid.ToString());
        entry.Write("path", path.generic_string());
    }
}
void Pitaya::Asset::AssetHub::AssetRegistry::Deserialize(const Pitaya::Serialize::DeserializeContext& context)
{
    Map.Clear();
    for (const auto& key : context.GetAllSubKeys())
    {
        Pitaya::Core::GUID guid(key);
        if (!guid) { continue; }

        const auto& entry = context.GetSubContext(key);
        std::string path;
        if (!entry.Read("path", path) || path.empty()) { continue; }
        Map.Insert(guid, std::filesystem::path(path));
    }
}
//ENDTODO
