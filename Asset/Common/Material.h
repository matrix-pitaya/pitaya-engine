#pragma once

#include<Core/Color/Color.h>
#include<Core/Identifier/GUID.h>
#include<Core/Asset/AssetRef.h>

#include<Serialize/Serializable.h>

#include<Asset/Common/FuncTable.h>
#include<Asset/Common/Texture.h>
#include<Asset/Common/Shader.h>
#include<Render/Common/RenderQueue.h>

#include<glm.hpp>
#include<cstdint>
#include<atomic>
#include<vector>
#include<type_traits>
#include<cstring>

namespace Pitaya::Asset
{
    struct Material : public Pitaya::Serialize::Serializable
    {
        Pitaya::Core::AssetRef<Pitaya::Asset::Shader> Shader = nullptr;
        std::vector<float> FloatParams;
        std::vector<glm::vec4> VectorParams;
        std::vector<Pitaya::Core::AssetRef<Pitaya::Asset::Texture>> Textures;

        Pitaya::Render::RenderQueue RenderQueue = Pitaya::Render::RenderQueue::Geometry;
        const uint32_t SortId = Next();
        uint8_t DrawOrder = 0;
        bool DepthTest = true;
        bool Blend = false;
        bool CullFace = true;

    public:
        inline static constexpr const Pitaya::Core::GUID PBR = Pitaya::Core::GUID("00000000-0000-0001-0000-000000000001");

    public:
        void SetFloat(std::string_view name, float value)
        {
            if (const auto* slot = FindSlot(name))
            {
                if (slot->Index < FloatParams.size()) 
                { 
                    FloatParams[slot->Index] = value;
                }
            }
        }
        void SetVec4(std::string_view name, glm::vec4 value)
        {
            if (const auto* slot = FindSlot(name))
            {
                if (slot->Index < VectorParams.size()) 
                { 
                    VectorParams[slot->Index] = value; 
                }
            }
        }
        void SetTexture(std::string_view name, Pitaya::Core::GUID guid)
        {
            if (const auto* slot = FindSlot(name))
            {
                if (slot->Index >= Textures.size())
                {
                    Textures.resize(slot->Index + 1);
                }
                Textures[slot->Index] = Pitaya::Asset::LoadAsset<Pitaya::Asset::Texture>(guid);
            }
        }

    private:
        inline static uint32_t Next() noexcept
        {
            static std::atomic<uint32_t> sortId = 1;
            return sortId.fetch_add(1, std::memory_order_relaxed);
        }
        inline const ParamSlot* FindSlot(std::string_view name) const
        {
            if (Shader && Shader->ParamLayout.TotalBytes != 0)
            {
                for (const auto& slot : Shader->ParamLayout.Slots)
                {
                    if (slot.Name == name) 
                    {  
                        return &slot;
                    }
                }
            }
            return nullptr;
        }

    private:
        void Serialize(Pitaya::Serialize::SerializeContext& context) const override
        {
            auto& shader = context.GetSubContext("Shader");
            shader.Write("GUID", Shader.GetGUID().ToString());

            if (Shader && !Shader->ParamLayout.Slots.empty())
            {
                auto& textures = context.GetSubContext("Textures");
                for (auto& slot : Shader->ParamLayout.Slots)
                {
                    if (slot.Type != ParamType::Texture || slot.Index >= Textures.size() || !Textures[slot.Index]) 
                    { continue; }

                    auto& tex = textures.GetSubContext(slot.Name);
                    tex.Write("GUID", Textures[slot.Index].GetGUID().ToString());
                }
            }
        }
        void Deserialize(const Pitaya::Serialize::DeserializeContext& context) override
        {
            std::string str;
            if (context.HasSubContext("Shader"))
            {
                auto& shader = context.GetSubContext("Shader");
                if (shader.Read("GUID", str))
                {
                    if (Pitaya::Core::GUID guid = Pitaya::Core::GUID(str))
                    {
                        Shader = Pitaya::Asset::LoadAsset<Pitaya::Asset::Shader>(guid);
                    }
                }
            }

            if (!Shader) { return; }

            FloatParams.resize(Shader->ParamLayout.FloatCount);
            VectorParams.resize(Shader->ParamLayout.VectorCount);
            Textures.resize(Shader->ParamLayout.TextureCount);

            if (!context.HasSubContext("Textures")) { return; }

            const auto& textures = context.GetSubContext("Textures");
            for (const auto& slot : Shader->ParamLayout.Slots)
            {
                if (slot.Type != ParamType::Texture || slot.Name == nullptr || slot.Index >= Textures.size())
                {
                    continue;
                }

                if (!textures.HasSubContext(slot.Name)) { continue; }

                std::string textureGUID;
                const auto& texture = textures.GetSubContext(slot.Name);
                if (texture.Read("GUID", textureGUID))
                {
                    if (Pitaya::Core::GUID guid = Pitaya::Core::GUID(textureGUID))
                    {
                        Textures[slot.Index] = Pitaya::Asset::LoadAsset<Pitaya::Asset::Texture>(guid);
                    }
                }
            }
        }
    };
}
