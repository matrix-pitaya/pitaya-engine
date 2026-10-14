#pragma once

#include<Asset/Common/AssetType.h>

#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Identifier/GUID.h>

#include<cstdint>
#include<string_view>

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

    struct BuiltinAssetInfo
    {
        AssetTypeId Type = AssetTypeId::Invalid;
        std::string_view Name;
    };

    template<> struct AssetTraits<Texture> { static constexpr AssetTypeId Type = AssetTypeId::Texture; };
    template<> struct AssetTraits<Material> { static constexpr AssetTypeId Type = AssetTypeId::Material; };
    template<> struct AssetTraits<Mesh> { static constexpr AssetTypeId Type = AssetTypeId::Mesh; };
    template<> struct AssetTraits<Shader> { static constexpr AssetTypeId Type = AssetTypeId::Shader; };
    template<> struct AssetTraits<RenderTarget> { static constexpr AssetTypeId Type = AssetTypeId::RenderTarget; };
    template<> struct AssetTraits<SkyBox> { static constexpr AssetTypeId Type = AssetTypeId::SkyBox; };

    class BuiltinAssetCatalog
    {
    public:
        template<AssetType T>
        inline void Register(Pitaya::Core::GUID guid, std::string_view name)
        {
            entries.InsertOrAssign(guid, BuiltinAssetInfo{ AssetTraits<T>::Type, name });
        }

        inline bool Find(Pitaya::Core::GUID guid, BuiltinAssetInfo& out) const
        {
            bool found = false;
            entries.FindAndReadKV(guid,
                [&found, &out](Pitaya::Core::GUID, const BuiltinAssetInfo& info)
                {
                    out = info;
                    found = true;
                },
                [](Pitaya::Core::GUID) {});
            return found;
        }

        template<AssetType T>
        inline bool Matches(Pitaya::Core::GUID guid) const
        {
            BuiltinAssetInfo info;
            return Find(guid, info) && info.Type == AssetTraits<T>::Type;
        }

    private:
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID, BuiltinAssetInfo> entries;
    };
}
