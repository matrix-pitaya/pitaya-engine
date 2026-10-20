#include<Serialize/Serializable.h>
#include<Serialize/Backend/YAML/YAMLSerializeContext.h>
#include<Log/Common/FuncTable.h>

namespace
{
    inline std::unique_ptr<Pitaya::Serialize::SerializeContext> CreateSerializeContext(Pitaya::Serialize::API api)
    {
        switch (api)
        {
            case Pitaya::Serialize::API::YAML:
                return std::make_unique<Pitaya::Serialize::YAMLSerializeContext>();

            case Pitaya::Serialize::API::Unknown:
                Pitaya::Log::Error("unknown serialize api, create yaml serialize context");
                return std::make_unique<Pitaya::Serialize::YAMLSerializeContext>();

            default:
                Pitaya::Log::Error("unknown serialize api, create yaml serialize context");
                return std::make_unique<Pitaya::Serialize::YAMLSerializeContext>();
        }
    }
    inline std::unique_ptr<Pitaya::Serialize::DeserializeContext> CreateDeserializeContext(Pitaya::Serialize::API api)
    {
        switch (api)
        {
            case Pitaya::Serialize::API::YAML:
                return std::make_unique<Pitaya::Serialize::YAMLDeserializeContext>();

            case Pitaya::Serialize::API::Unknown:
                Pitaya::Log::Error("unknown serialize api, create yaml deserialize context");
                return std::make_unique<Pitaya::Serialize::YAMLDeserializeContext>();

            default:
                Pitaya::Log::Error("unknown serialize api, create yaml deserialize context");
                return std::make_unique<Pitaya::Serialize::YAMLDeserializeContext>();
        }
    }
}

bool Pitaya::Serialize::Serializable::SerializeToFile(const std::filesystem::path& file, Pitaya::Serialize::API api) const
{
    std::unique_ptr<SerializeContext> context = CreateSerializeContext(api);
    if (!context)
    {
        Pitaya::Log::Error("serialize context create fail, path:" + file.string());
        return false;
    }

    try 
    { 
        context->OpenFile(file);
        Serialize(*context);
        context->Save();
        return true;
    }
    catch (const std::exception& e)
    {
        Pitaya::Log::Error("serialize fail, path:" + file.string() + ", error:" + e.what());
        return false;
    }
}
bool Pitaya::Serialize::Serializable::DeserializeFromFile(const std::filesystem::path& file, Pitaya::Serialize::API api)
{
    std::unique_ptr<DeserializeContext> context = CreateDeserializeContext(api);
    if (!context)
    {
        Pitaya::Log::Error("deseria context create fail, path:" + file.string());
        return false;
    }

    try
    {
        context->LoadFile(file);
        Deserialize(*context);
        return true;
    }
    catch (const std::exception& e)
    {
        Pitaya::Log::Error("deserialize fail, path:" + file.string() + ", error:" + e.what());
        return false;
    }
}
bool Pitaya::Serialize::Serializable::SerializeToString(std::string& str, Pitaya::Serialize::API api) const
{
    std::unique_ptr<SerializeContext> context = CreateSerializeContext(api);
    if (!context)
    {
        Pitaya::Log::Error("serialize context create fail");
        return false;
    }

    try
    {
        Serialize(*context);
        str = context->ToString();
        return true;
    }
    catch (const std::exception& e)
    {
        Pitaya::Log::Error(std::string("serialize fail, error:") + e.what());
        return false;
    }
}
bool Pitaya::Serialize::Serializable::DeserializeFromString(const std::string& data, Pitaya::Serialize::API api)
{
    std::unique_ptr<DeserializeContext> context = CreateDeserializeContext(api);
    if (!context)
    {
        Pitaya::Log::Error("deserialize context create fail");
        return false;
    }

    try
    {
         context->LoadString(data);
         Deserialize(*context);
         return true;
    }
    catch (const std::exception& e)
    {
        Pitaya::Log::Error(std::string("deserialize fail, load string error, error:") + e.what());
        return false;
    }
}
