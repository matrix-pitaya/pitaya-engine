#pragma once

#include<Core/Math/AABB.h>
#include<Core/Identifier/GUID.h>
#include<Core/Container/SlotMap.h>
#include<GPU/Common/Identifier.h>
#include<GPU/Common/MeshType.h>
#include<Asset/Common/Material.h>

#include<vector>
#include<glm.hpp>

namespace Pitaya::Asset
{
	struct Mesh
	{
		struct SubMesh
		{
			uint32_t IndexCount = 0;         //索引数量
			uint32_t BaseIndex = 0;          //EBO的起始位置
			uint32_t BaseVertex = 0;         //VBO的顶点偏移
			uint32_t MaterialIndex = 0;
			Pitaya::Core::AABB BoundingBox;
		};

		Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle VertexArrayHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle VertexBufferHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle IndexBufferHandle;
		std::vector<SubMesh> SubMeshs;
		Pitaya::Core::AABB BoundingBox;

		std::vector<Pitaya::Core::AssetRef<Pitaya::Asset::Material>> Materials;
		Pitaya::GPU::MeshType Type = Pitaya::GPU::MeshType::Static;

		//骨骼数据专属
		uint32_t BoneCount = 0;
		std::vector<glm::mat4> BoneInverseMatrices;

		//Mesh 25-32
		inline static constexpr const Pitaya::Core::GUID Cube = Pitaya::Core::GUID("00000001-0000-0000-0000-000000000000");
		inline static constexpr const Pitaya::Core::GUID Sphere = Pitaya::Core::GUID("00000002-0000-0000-0000-000000000000");
		inline static constexpr const Pitaya::Core::GUID Panel = Pitaya::Core::GUID("00000003-0000-0000-0000-000000000000");
	};
}
