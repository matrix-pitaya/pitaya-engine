#include<Asset/Cook/BuildManifest.h>

#include<string>

namespace
{
    bool IsValidAssetType(uint32_t value) noexcept
    {
        return value < static_cast<uint32_t>(Pitaya::Asset::AssetTypeId::Invalid);
    }
}

bool Pitaya::Asset::BuildManifest::Initialize(const std::filesystem::path& manifestFilePath)
{
    manifestFile = manifestFilePath;
    Clear();
    if (!std::filesystem::exists(manifestFile)) { return true; }
    return DeserializeFromFile(manifestFile);
}

bool Pitaya::Asset::BuildManifest::Save() const
{
    return !manifestFile.empty() && SerializeToFile(manifestFile);
}

void Pitaya::Asset::BuildManifest::Release()
{
    Save();
    Clear();
    manifestFile.clear();
}

void Pitaya::Asset::BuildManifest::Clear()
{
    entries.Clear();
    entryKeys.Clear();
}

bool Pitaya::Asset::BuildManifest::Register(const BuildManifestEntry& entry)
{
    if (!entry.Key.Id || !IsValidAssetType(static_cast<uint32_t>(entry.Key.Type))
        || entry.ArtifactPath.empty())
    {
        return false;
    }

    AssetKey previousKey;
    if (GetEntryKeyByGUID(entry.Key.Id, previousKey) && previousKey != entry.Key)
    {
        entries.Erase(previousKey);
    }

    entries.InsertOrAssign(entry.Key, entry);
    entryKeys.InsertOrAssign(entry.Key.Id, entry.Key);
    return true;
}

bool Pitaya::Asset::BuildManifest::Find(
    const AssetKey& key, BuildManifestEntry& outEntry) const
{
    bool found = false;
    entries.FindAndReadKV(key,
        [&found, &outEntry](const AssetKey&, const BuildManifestEntry& entry)
        {
            outEntry = entry;
            found = true;
        },
        [](const AssetKey&) {});
    return found;
}

bool Pitaya::Asset::BuildManifest::FindByGUID(
    Pitaya::Core::GUID guid, BuildManifestEntry& outEntry) const
{
    AssetKey key;
    return GetEntryKeyByGUID(guid, key) && Find(key, outEntry);
}

bool Pitaya::Asset::BuildManifest::GetArtifactPath(
    const AssetKey& key, std::filesystem::path& outPath) const
{
    BuildManifestEntry entry;
    if (!Find(key, entry)) { return false; }
    outPath = entry.ArtifactPath;
    return true;
}

bool Pitaya::Asset::BuildManifest::GetEntryKeyByGUID(
    Pitaya::Core::GUID guid, AssetKey& outKey) const
{
    bool found = false;
    entryKeys.FindAndReadKV(guid,
        [&found, &outKey](Pitaya::Core::GUID, const AssetKey& key)
        {
            outKey = key;
            found = true;
        },
        [](Pitaya::Core::GUID) {});
    return found;
}

void Pitaya::Asset::BuildManifest::Serialize(
    Pitaya::Serialize::SerializeContext& context) const
{
    context.Write("schema_version", schemaVersion);

    for (const auto& [guid, key] : entryKeys.GetAllKeyValuePairs())
    {
        BuildManifestEntry entry;
        if (!Find(key, entry)) { continue; }

        auto& serialized = context.GetSubContext(guid.ToString());
        serialized.Write("artifact_path", entry.ArtifactPath.generic_string());
        serialized.Write("artifact_hash", std::to_string(entry.ArtifactHash));
        serialized.Write("type", static_cast<uint32_t>(entry.Key.Type));
        serialized.Write("chunk", entry.Chunk);

        auto& dependencies = serialized.GetSubContext("dependencies");
        for (size_t index = 0; index < entry.Dependencies.size(); ++index)
        {
            auto& dependency = dependencies.GetSubContext(std::to_string(index));
            dependency.Write("guid", entry.Dependencies[index].Id.ToString());
            dependency.Write("type", static_cast<uint32_t>(entry.Dependencies[index].Type));
        }
    }
}

void Pitaya::Asset::BuildManifest::Deserialize(
    const Pitaya::Serialize::DeserializeContext& context)
{
    Clear();

    for (const auto& key : context.GetAllSubKeys())
    {
        Pitaya::Core::GUID guid(key);
        if (!guid) { continue; }

        const auto& serialized = context.GetSubContext(key);
        std::string artifactPath;
        if (!serialized.Read("artifact_path", artifactPath) || artifactPath.empty()) { continue; }

        uint32_t type = static_cast<uint32_t>(AssetTypeId::Invalid);
        if (!serialized.Read("type", type) || !IsValidAssetType(type)) { continue; }

        BuildManifestEntry entry;
        entry.Key = { guid, static_cast<AssetTypeId>(type) };
        entry.ArtifactPath = std::filesystem::path(artifactPath);
        serialized.Read("chunk", entry.Chunk);

        std::string hash;
        if (serialized.Read("artifact_hash", hash) && !hash.empty())
        {
            try { entry.ArtifactHash = std::stoull(hash); }
            catch (...) { entry.ArtifactHash = 0; }
        }

        if (serialized.HasSubContext("dependencies"))
        {
            const auto& dependencies = serialized.GetSubContext("dependencies");
            for (const auto& dependencyKey : dependencies.GetAllSubKeys())
            {
                const auto& dependency = dependencies.GetSubContext(dependencyKey);
                std::string dependencyGUID;
                if (!dependency.Read("guid", dependencyGUID)) { continue; }

                Pitaya::Core::GUID dependencyId(dependencyGUID);
                if (!dependencyId) { continue; }

                uint32_t dependencyType = static_cast<uint32_t>(AssetTypeId::Invalid);
                if (!dependency.Read("type", dependencyType) || !IsValidAssetType(dependencyType)) { continue; }
                entry.Dependencies.push_back({ dependencyId, static_cast<AssetTypeId>(dependencyType) });
            }
        }

        Register(entry);
    }
}
