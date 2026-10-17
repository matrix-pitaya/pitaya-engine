#pragma once

#include<glm.hpp>

#include<filesystem>
#include<string>
#include<vector>

namespace Pitaya::Import
{
    struct MaterialFloatProperty
    {
        std::string Name;
        float Value = 0.0f;
    };

    struct MaterialVectorProperty
    {
        std::string Name;
        glm::vec4 Value = glm::vec4(0.0f);
    };

    struct MaterialTextureProperty
    {
        std::string Name;
        std::filesystem::path SourcePath;
    };

    struct MaterialImportSource
    {
        std::string Name;
        std::vector<MaterialFloatProperty> FloatProperties;
        std::vector<MaterialVectorProperty> VectorProperties;
        std::vector<MaterialTextureProperty> TextureProperties;
    };
}
