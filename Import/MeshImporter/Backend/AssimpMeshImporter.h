#pragma once

#include<Import/MeshImporter/Frontend/MeshImporter.h>
#include<GPU/Common/TextureSlot.h>

#include<assimp/scene.h>

namespace Pitaya::Asset
{
	struct Material;
}

namespace Pitaya::Import
{
	class AssimpMeshImporter final : public MeshImporter
	{
        struct AssimpTextureInfo
        {
            aiTextureType Type;  
            uint32_t Index;      
            std::string FilePath;
            int UVIndex;         
            float BlendFactor;   
        };

	public:
		AssimpMeshImporter() = default;
		~AssimpMeshImporter()override = default;

	public:
		bool Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, Pitaya::Import::MeshPreloadResult& out, const Pitaya::Import::ImportContext* context = nullptr) override;
		bool Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::StaticMeshImportResult& out, const Pitaya::Import::ImportContext* context = nullptr) override;
		bool Import(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& preload, Pitaya::Import::SkinnedMeshImportResult& out, const Pitaya::Import::ImportContext* context = nullptr) override;
	
	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform, std::vector<Pitaya::Import::MeshPreloadResult::MeshNodeInfo>& outMeshInfos);
		bool ParseMaterial(const aiMaterial* aimaterial, const std::filesystem::path& matFilePath, const std::filesystem::path& modelFilePath, const Pitaya::Import::ImportContext* context);

	private:
		inline Pitaya::GPU::TextureSlot AiTextureTypeToTextureUsage(aiTextureType aiType) const noexcept
		{
            switch (aiType)
            {
                // 基础色
                case aiTextureType_BASE_COLOR:
                case aiTextureType_DIFFUSE:
                    return Pitaya::GPU::TextureSlot::Albedo;

                // 高光
                case aiTextureType_SPECULAR:
                    return Pitaya::GPU::TextureSlot::Specular;

                // 法线
                case aiTextureType_NORMALS:
                case aiTextureType_NORMAL_CAMERA:
                    return Pitaya::GPU::TextureSlot::Normal;

                // 金属度
                case aiTextureType_METALNESS:
                    return Pitaya::GPU::TextureSlot::Metallic;

                // 粗糙度
                case aiTextureType_DIFFUSE_ROUGHNESS:
                    return Pitaya::GPU::TextureSlot::Roughness;

                // 环境光遮蔽
                case aiTextureType_AMBIENT_OCCLUSION:
                case aiTextureType_LIGHTMAP:
                    return Pitaya::GPU::TextureSlot::AmbientOcclusion;

                // 自发光 (注意：你的枚举是 Emission)
                case aiTextureType_EMISSIVE:
                case aiTextureType_EMISSION_COLOR:
                    return Pitaya::GPU::TextureSlot::Emission;

                // 高度/位移
                case aiTextureType_HEIGHT:
                case aiTextureType_DISPLACEMENT:
                    return Pitaya::GPU::TextureSlot::Height;

                // 遮罩 (通常用 Opacity 或 Shininess 贴图充当)
                case aiTextureType_OPACITY:
                case aiTextureType_SHININESS:
                    return Pitaya::GPU::TextureSlot::Mask;

                default:
                    return static_cast<Pitaya::GPU::TextureSlot>(Pitaya::GPU::MaterialTextureSlotCount);
            }
		}
        inline std::filesystem::path GetTextureAbsolutePath(const std::filesystem::path& modelFilePath, const aiString& aiPath)
        {
            std::string texPathStr = aiPath.C_Str();

            //TODO 嵌入纹理 需要单独aiScene::GetEmbeddedTexture处理
            if (texPathStr.empty() || texPathStr[0] == '*') { return std::filesystem::path(); }

            //转为 path 清理 "/" 和 "\\" 混用
            std::filesystem::path texPath = texPathStr;

            //拼接 模型所在目录 + 纹理相对路径
            std::filesystem::path modelDir = modelFilePath.parent_path();
            std::filesystem::path absoluteTexPath = modelDir / texPath;

            return absoluteTexPath;
        }
    };
}
