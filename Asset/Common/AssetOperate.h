#pragma once

#include<Core/Container/SlotMap.h>
#include<Import/Common/ImportResult.h>

namespace Pitaya::Asset
{
	struct Texture2DUnloadRequire
	{
		Pitaya::Core::SlotMap<Pitaya::GPU::Texture2D>::Handle Texture2DHandle;
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
			Pitaya::Asset::Texture2DUnloadRequire> Data;
		uint64_t Generation = 0;
	};
}
