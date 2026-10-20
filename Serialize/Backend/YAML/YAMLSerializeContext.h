#pragma once

#include<Serialize/Frontend/SerializeContext.h>
#include<Serialize/Frontend/DeserializeContext.h>

#include<yaml-cpp/yaml.h>

#include<unordered_map>

namespace Pitaya::Serialize
{
	class YAMLSerializeContext : public SerializeContext
	{
    public:
        YAMLSerializeContext() = default;
        ~YAMLSerializeContext() override = default;
        YAMLSerializeContext(YAML::Node parent, const std::string& key)
            :isSubContext(true)
        {
            root = parent[key];
        }

    public:
        void OpenFile(const std::filesystem::path& file) override;
        void Save() override;

    public:
        SerializeContext& GetSubContext(const std::string& key) override;

    public:
        void Write(const std::string& key, const char* value) override;
        void Write(const std::string& key, const std::string& value) override;
        void Write(const std::string& key, float value) override;
        void Write(const std::string& key, glm::vec2 value) override;
        void Write(const std::string& key, glm::vec3 value) override;
        void Write(const std::string& key, glm::vec4 value) override;
        void Write(const std::string& key, int value) override;
        void Write(const std::string& key, glm::ivec2 value) override;
        void Write(const std::string& key, glm::ivec3 value) override;
        void Write(const std::string& key, glm::ivec4 value) override;
        void Write(const std::string& key, bool value) override;
        void Write(const std::string& key, uint32_t value) override;
        void Write(const std::string& key, uint64_t value) override;

    public:
        std::string ToString() const override;

    private:
        YAML::Node root;
        std::filesystem::path file;
        std::unordered_map<std::string, std::unique_ptr<YAMLSerializeContext>> subContexts;
        bool isSubContext = false;
	};
	class YAMLDeserializeContext : public DeserializeContext
	{
    public:
        YAMLDeserializeContext() = default;
        ~YAMLDeserializeContext() override = default;
        YAMLDeserializeContext(YAML::Node parent, const std::string& key)
            :isSubContext(true)
        {
            root = parent[key];
        }

    public:
        void LoadFile(const std::filesystem::path& file) override;
        void LoadString(const std::string& data) override;

    public:
        const DeserializeContext& GetSubContext(const std::string& key) const override;
        bool HasSubContext(const std::string& key) const override;
        std::vector<std::string> GetAllSubKeys() const override;

    public:
       bool Read(const std::string& key, std::string& out) const override;
       bool Read(const std::string& key, float& out) const override;
       bool Read(const std::string& key, glm::vec2& out) const override;
       bool Read(const std::string& key, glm::vec3& out) const override;
       bool Read(const std::string& key, glm::vec4& out) const override;
       bool Read(const std::string& key, int& out) const override;
       bool Read(const std::string& key, glm::ivec2& out) const override;
       bool Read(const std::string& key, glm::ivec3& out) const override;
       bool Read(const std::string& key, glm::ivec4& out) const override;
       bool Read(const std::string& key, bool& out) const override;
       bool Read(const std::string& key, uint32_t& out) const override;
       bool Read(const std::string& key, uint64_t& out) const override;

    private:     
        YAML::Node root;
        mutable std::unordered_map<std::string, std::unique_ptr<YAMLDeserializeContext>> subContexts;
        bool isSubContext = false;
	};
}
