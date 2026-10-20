#pragma once

#include<filesystem>
#include<cstdint>
#include<string>
#include<vector>

#include<glm.hpp>
#include<gtc/quaternion.hpp>

namespace Pitaya::Serialize
{
    class SerializeContext
    {
    public:
        SerializeContext() = default;
        virtual ~SerializeContext() = default;

    public:
        virtual void OpenFile(const std::filesystem::path& file) = 0;
        virtual void Save() = 0;

    public:
        virtual SerializeContext& GetSubContext(const std::string& key) = 0;

    public:
        virtual void Write(const std::string& key, const std::string& value) = 0;
        virtual void Write(const std::string& key, const char* value) = 0;
        virtual void Write(const std::string& key, float value) = 0;
        virtual void Write(const std::string& key, glm::vec2 value) = 0;
        virtual void Write(const std::string& key, glm::vec3 value) = 0;
        virtual void Write(const std::string& key, glm::vec4 value) = 0;
        virtual void Write(const std::string& key, int value) = 0;
        virtual void Write(const std::string& key, glm::ivec2 value) = 0;
        virtual void Write(const std::string& key, glm::ivec3 value) = 0;
        virtual void Write(const std::string& key, glm::ivec4 value) = 0;
        virtual void Write(const std::string& key, bool value) = 0;
        virtual void Write(const std::string& key, uint32_t value) = 0;
        virtual void Write(const std::string& key, uint64_t value) = 0;

    public:
        virtual std::string ToString() const = 0;
    };
}
