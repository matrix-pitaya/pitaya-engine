#pragma once

#include<Core/Container/SlotMap.h>
#include<Import/Common/ImportResult.h>

#include<GPU/Frontend/Buffer/FrameBuffer.h>
#include<GPU/Frontend/Buffer/IndexBuffer.h>
#include<GPU/Frontend/Buffer/VertexArray.h>
#include<GPU/Frontend/Buffer/VertexBuffer.h>
#include<GPU/Frontend/Shader/Shader.h>
#include<GPU/Frontend/Texture/Texture2D.h>
#include<GPU/Frontend/Texture/Texture2DArray.h>
#include<GPU/Frontend/Texture/TextureCubemap.h>

namespace Pitaya::Asset
{
	struct Texture2DUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle Texture2DHandle;
	};

	struct Texture2DArrayUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::Texture2DArray>::Handle Texture2DArrayHandle;
	};

	struct TextureCubemapUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle TextureCubemapHandle;
	};

	struct ShaderUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::Shader>::Handle ShaderHandle;
	};

	struct MeshUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::VertexArray>::Handle VertexArrayHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::VertexBuffer>::Handle VertexBufferHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::IndexBuffer>::Handle IndexBufferHandle;
	};

	struct RenderTargetUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle SceneFrameBufferHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle PingPongFrameBufferHandles[2] = { };
		Pitaya::Core::SlotMap<Pitaya::GPU::FrameBuffer>::Handle FinalFrameBufferHandle;
	};

	struct SkyBoxUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle EquirectHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle EnvCubemapHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle IrradianceHandle;
		Pitaya::Core::SlotMap<Pitaya::GPU::TextureCubemap>::Handle PrefilteredHandle;
	};

	struct AssetOperate
	{
		std::variant<std::monostate,
			Pitaya::Import::Texture2DImportResult,
			Pitaya::Import::TextureCubemapImportResult,
			Pitaya::Import::ShaderImportResult,
			Pitaya::Import::StaticMeshImportResult,
			Pitaya::Import::SkinnedMeshImportResult,
			Pitaya::Import::RenderTargetImportResult,
			Pitaya::Import::SkyBoxImportResult,
			Pitaya::Asset::Texture2DUnloadRequire,
			Pitaya::Asset::Texture2DArrayUnloadRequire,
			Pitaya::Asset::TextureCubemapUnloadRequire,
			Pitaya::Asset::ShaderUnloadRequire,
			Pitaya::Asset::MeshUnloadRequire,
			Pitaya::Asset::RenderTargetUnloadRequire,
			Pitaya::Asset::SkyBoxUnloadRequire> Data;
		uint64_t Generation = 0;
	};
}
