#pragma once

#include<type_traits>

namespace Pitaya::Asset
{
	struct Texture;
	struct Material;
	struct Mesh;
	struct Shader;
	struct RenderTarget;
	struct SkyBox;

	template<typename T>
	concept AssetType =
		std::is_same_v<T, Texture> ||
		std::is_same_v<T, Material> ||
		std::is_same_v<T, Mesh> ||
		std::is_same_v<T, Shader> ||
		std::is_same_v<T, RenderTarget> ||
		std::is_same_v<T, SkyBox>;

	template<typename T>
	struct AssetTraits;
}

template<typename T>
concept AssetType = Pitaya::Asset::AssetType<T>;
