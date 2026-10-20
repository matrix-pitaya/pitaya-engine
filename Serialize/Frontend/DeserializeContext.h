#pragma once

#include<filesystem>
#include<cstdint>
#include<string>
#include<vector>

#include<glm.hpp>
#include<gtc/quaternion.hpp>

namespace Pitaya::Serialize
{
    class DeserializeContext
    {
    public:
        DeserializeContext() = default;
        virtual ~DeserializeContext() = default;

    public:
        virtual void LoadFile(const std::filesystem::path& file) = 0;
        virtual void LoadString(const std::string& data) = 0;

    public:
        virtual const DeserializeContext& GetSubContext(const std::string& key) const = 0;
        virtual bool HasSubContext(const std::string& key) const = 0;
        virtual std::vector<std::string> GetAllSubKeys() const = 0;

    public:
        virtual bool Read(const std::string& key, std::string& out) const = 0;
        virtual bool Read(const std::string& key, float& out) const = 0;
        virtual bool Read(const std::string& key, glm::vec2& out) const = 0;
        virtual bool Read(const std::string& key, glm::vec3& out) const = 0;
        virtual bool Read(const std::string& key, glm::vec4& out) const = 0;
        virtual bool Read(const std::string& key, int& out) const = 0;
        virtual bool Read(const std::string& key, glm::ivec2& out) const = 0;
        virtual bool Read(const std::string& key, glm::ivec3& out) const = 0;
        virtual bool Read(const std::string& key, glm::ivec4& out) const = 0;
        virtual bool Read(const std::string& key, bool& out) const = 0;
        virtual bool Read(const std::string& key, uint32_t& out) const = 0;
        virtual bool Read(const std::string& key, uint64_t& out) const = 0;
    };
}
