#pragma once

#include<Asset/Common/AssetKey.h>

#include<Core/Container/ThreadSafe/ThreadSafeBidirectionalMap.h>
#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>

#include<Serialize/Serializable.h>

#include<filesystem>

namespace Pitaya::Asset
{
    class AssetDatabase final : public Pitaya::Serialize::Serializable
    {
    public:
        AssetDatabase() = default;
        ~AssetDatabase() override = default;

        AssetDatabase(const AssetDatabase&) = delete;
        AssetDatabase& operator=(const AssetDatabase&) = delete;
        AssetDatabase(AssetDatabase&&) = delete;
        AssetDatabase& operator=(AssetDatabase&&) = delete;

    public:
        bool Initialize(const std::filesystem::path& catalogFile);
        bool Save() const;
        void Release();

    public:
        bool GetAssetPathByGUID(Pitaya::Core::GUID guid, std::filesystem::path& outPath) const;
        bool GetAssetGUIDByPath(const std::filesystem::path& path, Pitaya::Core::GUID& outGUID) const;

        bool RegisterAssetPath(Pitaya::Core::GUID guid, const std::filesystem::path& path);
        bool RegisterAssetType(Pitaya::Core::GUID guid, AssetTypeId type);
        bool RegisterAsset(const AssetRecord& record);

        bool Find(const AssetKey& key, AssetRecord& outRecord) const;
        bool FindByGUID(Pitaya::Core::GUID guid, AssetRecord& outRecord) const;

    private:
        void Serialize(Pitaya::Serialize::SerializeContext&) const override;
        void Deserialize(const Pitaya::Serialize::DeserializeContext&) override;

        bool GetRecordKeyByGUID(Pitaya::Core::GUID guid, AssetKey& outKey) const;

    private:
        inline static constexpr uint32_t schemaVersion = 2;

        std::filesystem::path catalogFile;

        // Kept as a compatibility index for the existing AssetHub and FuncTable API.
        Pitaya::Core::ThreadSafeBidirectionalMap<Pitaya::Core::GUID, std::filesystem::path> paths;

        // The typed metadata index is populated alongside the legacy path index.
        Pitaya::Core::ThreadSafeHashMap<AssetKey, AssetRecord> records;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID, AssetKey> recordKeys;
    };
}
