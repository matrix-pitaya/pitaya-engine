#pragma once

#include<Asset/Common/AssetKey.h>

#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Identifier/GUID.h>

#include<Serialize/Serializable.h>

#include<cstdint>
#include<filesystem>
#include<vector>

namespace Pitaya::Asset
{
    // Cook 阶段写入的运行时产物索引。它不保存源文件路径，也不持有 GPU 对象。
    struct BuildManifestEntry
    {
        AssetKey Key;
        std::filesystem::path ArtifactPath;
        uint64_t ArtifactHash = 0;
        uint32_t Chunk = 0;
        std::vector<AssetKey> Dependencies;
    };

    class BuildManifest final : public Pitaya::Serialize::Serializable
    {
    public:
        BuildManifest() = default;
        ~BuildManifest() override = default;

        BuildManifest(const BuildManifest&) = delete;
        BuildManifest& operator=(const BuildManifest&) = delete;
        BuildManifest(BuildManifest&&) = delete;
        BuildManifest& operator=(BuildManifest&&) = delete;

    public:
        bool Initialize(const std::filesystem::path& manifestFilePath);
        bool Save() const;
        void Release();
        void Clear();

    public:
        bool Register(const BuildManifestEntry& entry);
        bool Find(const AssetKey& key, BuildManifestEntry& outEntry) const;
        bool FindByGUID(Pitaya::Core::GUID guid, BuildManifestEntry& outEntry) const;
        bool GetArtifactPath(const AssetKey& key, std::filesystem::path& outPath) const;

    private:
        void Serialize(Pitaya::Serialize::SerializeContext&) const override;
        void Deserialize(const Pitaya::Serialize::DeserializeContext&) override;

        bool GetEntryKeyByGUID(Pitaya::Core::GUID guid, AssetKey& outKey) const;

    private:
        inline static constexpr uint32_t schemaVersion = 1;

        std::filesystem::path manifestFile;
        Pitaya::Core::ThreadSafeHashMap<AssetKey, BuildManifestEntry> entries;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID, AssetKey> entryKeys;
    };
}
