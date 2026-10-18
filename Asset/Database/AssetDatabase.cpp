#include<Asset/Database/AssetDatabase.h>

#include<string>

namespace
{
    bool IsValidAssetType(uint32_t value) noexcept
    {
        return value <= static_cast<uint32_t>(Pitaya::Asset::AssetTypeId::Invalid);
    }

    bool IsValidAssetOrigin(uint32_t value) noexcept
    {
        return value <= static_cast<uint32_t>(Pitaya::Asset::AssetOrigin::Cooked);
    }
}

bool Pitaya::Asset::AssetDatabase::Initialize(const std::filesystem::path& catalogFilePath)
{
    catalogFile = catalogFilePath;
    paths.Clear();
    records.Clear();
    recordKeys.Clear();

    if (!std::filesystem::exists(catalogFile))
    {
        return true;
    }

    return DeserializeFromFile(catalogFile);
}

bool Pitaya::Asset::AssetDatabase::Save() const
{
    return !catalogFile.empty() && SerializeToFile(catalogFile);
}

void Pitaya::Asset::AssetDatabase::Release()
{
    Save();
}

bool Pitaya::Asset::AssetDatabase::GetAssetPathByGUID(
    Pitaya::Core::GUID guid, std::filesystem::path& outPath) const
{
    const auto path = paths.Get(guid);
    if (!path.has_value()) { return false; }

    outPath = path.value();
    return true;
}

bool Pitaya::Asset::AssetDatabase::GetAssetGUIDByPath(
    const std::filesystem::path& path, Pitaya::Core::GUID& outGUID) const
{
    const auto guid = paths.Get(path);
    if (!guid.has_value()) { return false; }

    outGUID = guid.value();
    return true;
}

bool Pitaya::Asset::AssetDatabase::RegisterAssetPath(
    Pitaya::Core::GUID guid, const std::filesystem::path& path)
{
    if (!guid || path.empty()) { return false; }

    AssetRecord record;
    if (!FindByGUID(guid, record))
    {
        record.Key = { guid, AssetTypeId::Invalid };
        record.Origin = AssetOrigin::Project;
    }
    record.SourcePath = path;
    return RegisterAsset(record);
}

bool Pitaya::Asset::AssetDatabase::RegisterAssetType(
    Pitaya::Core::GUID guid, AssetTypeId type)
{
    if (!guid || type == AssetTypeId::Invalid) { return false; }

    AssetRecord record;
    if (!FindByGUID(guid, record))
    {
        const auto path = paths.Get(guid);
        if (!path.has_value()) { return false; }

        record.Key = { guid, type };
        record.SourcePath = path.value();
        record.Origin = AssetOrigin::Project;
        return RegisterAsset(record);
    }

    if (record.Key.Type != AssetTypeId::Invalid && record.Key.Type != type)
    {
        return false;
    }

    record.Key.Type = type;
    return RegisterAsset(record);
}

bool Pitaya::Asset::AssetDatabase::RegisterAsset(const AssetRecord& record)
{
    if (!record.Key.Id) { return false; }

    AssetKey previousKey;
    if (GetRecordKeyByGUID(record.Key.Id, previousKey) && previousKey != record.Key)
    {
        records.Erase(previousKey);
    }

    records.InsertOrAssign(record.Key, record);
    recordKeys.InsertOrAssign(record.Key.Id, record.Key);

    if (!record.SourcePath.empty())
    {
        const auto previousGUID = paths.Get(record.SourcePath);
        if (previousGUID.has_value() && previousGUID.value() != record.Key.Id)
        {
            paths.Erase(previousGUID.value());
        }
        paths.Erase(record.Key.Id);
        paths.Insert(record.Key.Id, record.SourcePath);
    }
    return true;
}

bool Pitaya::Asset::AssetDatabase::Find(const AssetKey& key, AssetRecord& outRecord) const
{
    bool found = false;
    records.FindAndReadKV(key,
        [&found, &outRecord](const AssetKey&, const AssetRecord& record)
        {
            outRecord = record;
            found = true;
        },
        [](const AssetKey&) {});
    return found;
}

bool Pitaya::Asset::AssetDatabase::FindByGUID(
    Pitaya::Core::GUID guid, AssetRecord& outRecord) const
{
    AssetKey key;
    return GetRecordKeyByGUID(guid, key) && Find(key, outRecord);
}

bool Pitaya::Asset::AssetDatabase::GetRecordKeyByGUID(
    Pitaya::Core::GUID guid, AssetKey& outKey) const
{
    bool found = false;
    recordKeys.FindAndReadKV(guid,
        [&found, &outKey](Pitaya::Core::GUID, const AssetKey& key)
        {
            outKey = key;
            found = true;
        },
        [](Pitaya::Core::GUID) {});
    return found;
}

void Pitaya::Asset::AssetDatabase::Serialize(
    Pitaya::Serialize::SerializeContext& context) const
{
    context.Write("schema_version", schemaVersion);

    for (const auto& [guid, path] : paths.GetAllKeyValuePairs())
    {
        AssetRecord record;
        if (!FindByGUID(guid, record))
        {
            record.Key = { guid, AssetTypeId::Invalid };
            record.SourcePath = path;
        }

        auto& entry = context.GetSubContext(guid.ToString());
        entry.Write("path", path.generic_string());
        entry.Write("source_path", record.SourcePath.generic_string());
        entry.Write("artifact_path", record.ArtifactPath.generic_string());
        entry.Write("type", static_cast<uint32_t>(record.Key.Type));
        entry.Write("origin", static_cast<uint32_t>(record.Origin));
        entry.Write("flags", static_cast<uint32_t>(record.Flags));
        entry.Write("importer_version", record.ImporterVersion);
        entry.Write("source_hash", std::to_string(record.SourceHash));
        entry.Write("import_settings_hash", std::to_string(record.ImportSettingsHash));
        entry.Write("last_error", record.LastError);

        auto& dependencies = entry.GetSubContext("dependencies");
        for (size_t index = 0; index < record.Dependencies.size(); ++index)
        {
            auto& dependency = dependencies.GetSubContext(std::to_string(index));
            dependency.Write("guid", record.Dependencies[index].Id.ToString());
            dependency.Write("type", static_cast<uint32_t>(record.Dependencies[index].Type));
        }
    }
}

void Pitaya::Asset::AssetDatabase::Deserialize(
    const Pitaya::Serialize::DeserializeContext& context)
{
    paths.Clear();
    records.Clear();
    recordKeys.Clear();

    for (const auto& key : context.GetAllSubKeys())
    {
        Pitaya::Core::GUID guid(key);
        if (!guid) { continue; }

        const auto& entry = context.GetSubContext(key);
        std::string path;
        if (!entry.Read("source_path", path) || path.empty())
        {
            if (!entry.Read("path", path) || path.empty()) { continue; }
        }

        AssetRecord record;
        record.Key = { guid, AssetTypeId::Invalid };
        record.SourcePath = std::filesystem::path(path);

        std::string artifactPath;
        if (entry.Read("artifact_path", artifactPath) && !artifactPath.empty())
        {
            record.ArtifactPath = std::filesystem::path(artifactPath);
        }

        uint32_t value = 0;
        if (entry.Read("type", value) && IsValidAssetType(value))
        {
            record.Key.Type = static_cast<AssetTypeId>(value);
        }
        if (entry.Read("origin", value) && IsValidAssetOrigin(value))
        {
            record.Origin = static_cast<AssetOrigin>(value);
        }
        if (entry.Read("flags", value))
        {
            record.Flags = static_cast<AssetRecordFlags>(value);
        }
        entry.Read("importer_version", record.ImporterVersion);

        std::string hash;
        if (entry.Read("source_hash", hash) && !hash.empty())
        {
            try { record.SourceHash = std::stoull(hash); }
            catch (...) { record.SourceHash = 0; }
        }
        if (entry.Read("import_settings_hash", hash) && !hash.empty())
        {
            try { record.ImportSettingsHash = std::stoull(hash); }
            catch (...) { record.ImportSettingsHash = 0; }
        }
        entry.Read("last_error", record.LastError);

        if (entry.HasSubContext("dependencies"))
        {
            const auto& dependencies = entry.GetSubContext("dependencies");
            for (const auto& dependencyKey : dependencies.GetAllSubKeys())
            {
                const auto& dependency = dependencies.GetSubContext(dependencyKey);
                std::string dependencyGUID;
                if (!dependency.Read("guid", dependencyGUID)) { continue; }

                Pitaya::Core::GUID dependencyId(dependencyGUID);
                if (!dependencyId) { continue; }

                AssetKey dependencyAssetKey{ dependencyId, AssetTypeId::Invalid };
                if (dependency.Read("type", value) && IsValidAssetType(value))
                {
                    dependencyAssetKey.Type = static_cast<AssetTypeId>(value);
                }
                record.Dependencies.push_back(dependencyAssetKey);
            }
        }

        paths.Insert(guid, record.SourcePath);
        records.InsertOrAssign(record.Key, record);
        recordKeys.InsertOrAssign(guid, record.Key);
    }
}
