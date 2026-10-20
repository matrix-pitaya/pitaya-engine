#include<Serialize/Backend/YAML/YAMLSerializeContext.h>

#include<fstream>
#include<string>
#include<filesystem>

void Pitaya::Serialize::YAMLSerializeContext::OpenFile(const std::filesystem::path& file)
{
    if (isSubContext)
    {
        throw std::runtime_error("YAMLSerializeContext: Sub context cannot call OpenFile");
    }

	this->file = file;
	root = YAML::Node();
    subContexts.clear();
}
void Pitaya::Serialize::YAMLSerializeContext::Save()
{
    if (isSubContext)
    {
        throw std::runtime_error("YAMLSerializeContext: Sub context cannot call Save");
    }

    if (file.empty()) 
    { 
        throw std::runtime_error("YamlSerializeContext: No file path set");
    }


    const std::filesystem::path parentPath = file.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath))
    {
        std::filesystem::create_directories(parentPath);
    }

    const std::filesystem::path temporaryFile = file.string() + ".tmp";
    std::ofstream filestream(temporaryFile, std::ios::trunc);
    if (!filestream.is_open()) 
    { 
        throw std::runtime_error("YamlSerializeContext: Failed to open temporary file - " + temporaryFile.string()); 
    }

    filestream << YAML::Dump(root);
    filestream.flush();
    if (!filestream.good())
    {
        filestream.close();
        std::error_code cleanupError;
        std::filesystem::remove(temporaryFile, cleanupError);
        throw std::runtime_error("YamlSerializeContext: Failed to write file - " + file.string());
    }
    filestream.close();

    std::error_code renameError;
    std::filesystem::rename(temporaryFile, file, renameError);
    if (renameError)
    {
        std::error_code removeError;
        std::filesystem::remove(file, removeError);
        renameError.clear();
        std::filesystem::rename(temporaryFile, file, renameError);
    }
    if (renameError)
    {
        std::error_code cleanupError;
        std::filesystem::remove(temporaryFile, cleanupError);
        throw std::runtime_error("YamlSerializeContext: Failed to replace file - " + file.string());
    }
}
Pitaya::Serialize::SerializeContext& Pitaya::Serialize::YAMLSerializeContext::GetSubContext(const std::string& key)
{
    if (subContexts.count(key) == 0)
    {
        subContexts[key] = std::make_unique<YAMLSerializeContext>(root, key);
    }
    return *subContexts[key];
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, const std::string& value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, const char* value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, float value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::vec2 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::vec3 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::vec4 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, int value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::ivec2 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::ivec3 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, glm::ivec4 value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, bool value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, uint32_t value)
{
    root[key] = value;
}
void Pitaya::Serialize::YAMLSerializeContext::Write(const std::string& key, uint64_t value)
{
    root[key] = value;
}
std::string Pitaya::Serialize::YAMLSerializeContext::ToString() const
{
    return YAML::Dump(root);
}

void Pitaya::Serialize::YAMLDeserializeContext::LoadFile(const std::filesystem::path& file)
{
    if (isSubContext)
    {
        throw std::runtime_error("YAMLDeserializeContext: Sub context cannot call LoadFile");
    }

    if (!std::filesystem::exists(file))
    {
        throw std::runtime_error("YamlDeserializeContext: File not exists - " + file.string());
    }
        
    root = YAML::Node();
    subContexts.clear();
    try 
    {
        root = YAML::LoadFile(file.string());
    }
    catch (const YAML::Exception& e) 
    {
        throw std::runtime_error("YamlDeserializeContext: Parse failed - " + std::string(e.what()));
    }
}
void Pitaya::Serialize::YAMLDeserializeContext::LoadString(const std::string& data)
{
    if (isSubContext)
    {
        throw std::runtime_error("YAMLDeserializeContext: Sub context cannot call LoadString");
    }

    root = YAML::Node();
    subContexts.clear();
    try 
    {
        root = YAML::Load(data);
    }
    catch (const YAML::Exception& e) 
    {
        throw std::runtime_error("YamlDeserializeContext: Parse string failed - " + std::string(e.what()));
    }
}
const Pitaya::Serialize::DeserializeContext& Pitaya::Serialize::YAMLDeserializeContext::GetSubContext(const std::string& key) const
{
    if (subContexts.count(key) == 0)
    {
        subContexts[key] = std::make_unique<YAMLDeserializeContext>(root, key);
    }
    return *subContexts[key];
}
bool Pitaya::Serialize::YAMLDeserializeContext::HasSubContext(const std::string& key) const
{
    return root[key] && !root[key].IsNull();
}
std::vector<std::string> Pitaya::Serialize::YAMLDeserializeContext::GetAllSubKeys() const
{
    std::vector<std::string> keys;
    if (root.IsMap()) 
    {
        for (const auto& pair : root) 
        {
            if (pair.first.IsScalar()) 
            { 
                keys.push_back(pair.first.as<std::string>());
            }
        }
    }
    return keys;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, std::string& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<std::string>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, float& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<float>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::vec2& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::vec2>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::vec3& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::vec3>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::vec4& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::vec4>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, int& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<int>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::ivec2& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::ivec2>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::ivec3& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::ivec3>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, glm::ivec4& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<glm::ivec4>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, bool& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<bool>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, uint32_t& out) const 
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<uint32_t>();
        return true;
    }
    return false;
}
bool Pitaya::Serialize::YAMLDeserializeContext::Read(const std::string& key, uint64_t& out) const
{
    if (root[key] && root[key].IsScalar())
    {
        out = root[key].as<uint64_t>();
        return true;
    }
    return false;
}

namespace YAML 
{
    template<>
    struct convert<glm::vec2> 
    {
        static Node encode(const glm::vec2& rhs) 
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            return node;
        }

        static bool decode(const Node& node, glm::vec2& rhs) 
        {
            if (!node.IsSequence() || node.size() != 2) { return false; }
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            return true;
        }
    };
    template<>
    struct convert<glm::vec3> 
    {
        static Node encode(const glm::vec3& rhs) 
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& rhs) 
        {
            if (!node.IsSequence() || node.size() != 3) { return false; }
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            return true;
        }
    };
    template<>
    struct convert<glm::vec4> 
    {
        static Node encode(const glm::vec4& rhs) 
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs) 
        {
            if (!node.IsSequence() || node.size() != 4) { return false; }
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            return true;
        }
    };
    template<>
    struct convert<glm::quat> 
    {
        static Node encode(const glm::quat& rhs) 
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            return node;
        }

        static bool decode(const Node& node, glm::quat& rhs) 
        {
            if (!node.IsSequence() || node.size() != 4) { return false; }
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            return true;
        }
    };

    template<>
    struct convert<glm::ivec2>
    {
        static Node encode(const glm::ivec2& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            return node;
        }

        static bool decode(const Node& node, glm::ivec2& rhs)
        {
            if (!node.IsSequence() || node.size() != 2) { return false; }
            rhs.x = node[0].as<int>();
            rhs.y = node[1].as<int>();
            return true;
        }
    };
    template<>
    struct convert<glm::ivec3>
    {
        static Node encode(const glm::ivec3& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            return node;
        }

        static bool decode(const Node& node, glm::ivec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3) { return false; }
            rhs.x = node[0].as<int>();
            rhs.y = node[1].as<int>();
            rhs.z = node[2].as<int>();
            return true;
        }
    };
    template<>
    struct convert<glm::ivec4>
    {
        static Node encode(const glm::ivec4& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            return node;
        }

        static bool decode(const Node& node, glm::ivec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4) { return false; }
            rhs.x = node[0].as<int>();
            rhs.y = node[1].as<int>();
            rhs.z = node[2].as<int>();
            rhs.w = node[3].as<int>();
            return true;
        }
    };
}
