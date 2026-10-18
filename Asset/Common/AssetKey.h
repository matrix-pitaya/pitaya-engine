#pragma once

#include<Asset/Common/AssetType.h>

#include<Core/Identifier/GUID.h>

#include<cstdint>
#include<filesystem>
#include<functional>
#include<string>
#include<string_view>
#include<vector>

namespace Pitaya::Asset
{
	enum class AssetTypeId : uint8_t
	{
		Texture,
		Material,
		Mesh,
		Shader,
		RenderTarget,
		SkyBox,
		Invalid
	};

	enum class AssetOrigin : uint8_t
	{
		Engine,
		Project,
		Generated,
		Cooked
	};

	enum class AssetRecordFlags : uint32_t
	{
		None = 0,
		Builtin = 1 << 0,
		NeverUnload = 1 << 1,
		EditorOnly = 1 << 2
	};

	struct AssetKey
	{
		Pitaya::Core::GUID Id;
		AssetTypeId Type = AssetTypeId::Invalid;

		constexpr bool operator==(const AssetKey& other) const noexcept
		{
			return Id == other.Id && Type == other.Type;
		}
		constexpr bool operator!=(const AssetKey& other) const noexcept
		{
			return !(*this == other);
		}
	};

	struct AssetRecord
	{
		AssetKey Key;
		std::filesystem::path SourcePath;
		std::filesystem::path ArtifactPath;
		AssetOrigin Origin = AssetOrigin::Project;
		AssetRecordFlags Flags = AssetRecordFlags::None;
		uint32_t ImporterVersion = 0;
		uint64_t SourceHash = 0;
		uint64_t ImportSettingsHash = 0;
		std::vector<AssetKey> Dependencies;
		std::string LastError;
	};
}

namespace std
{
	template<>
	struct hash<Pitaya::Asset::AssetKey>
	{
		size_t operator()(const Pitaya::Asset::AssetKey& key) const noexcept
		{
			const size_t guidHash = std::hash<Pitaya::Core::GUID>{}(key.Id);
			const size_t typeHash = std::hash<uint8_t>{}(static_cast<uint8_t>(key.Type));
			return guidHash ^ (typeHash + 0x9e3779b9 + (guidHash << 6) + (guidHash >> 2));
		}
	};
}
