#pragma once

#include<Core/Identifier/GUID.h>
#include<Import/Common/ImportMaterial.h>

#include<filesystem>

namespace Pitaya::Import
{
    // Importers must not depend on AssetHub or AssetDatabase directly. The
    // coordinator above Import supplies these optional callbacks when an
    // importer needs to resolve a generated dependency.
    struct ImportContext
    {
        using TransformToVirtualPathFn = bool(*)(
            void*, const std::filesystem::path&, const std::filesystem::path&, std::filesystem::path&);
        using GetAssetGUIDByPathFn = bool(*)(void*, const std::filesystem::path&, Pitaya::Core::GUID&);
        using RegisterExternalFileFn = bool(*)(
            void*, const std::filesystem::path&, const std::filesystem::path&,
            std::filesystem::path&, Pitaya::Core::GUID&);
        using RegisterMaterialFn = bool(*)(
            void*, const std::filesystem::path&, const std::filesystem::path&,
            const MaterialImportSource&, Pitaya::Core::GUID&);

        void* UserData = nullptr;
        TransformToVirtualPathFn TransformToVirtualPath = nullptr;
        GetAssetGUIDByPathFn GetAssetGUIDByPath = nullptr;
        RegisterExternalFileFn RegisterExternalFile = nullptr;
        RegisterMaterialFn RegisterMaterial = nullptr;
        Pitaya::Core::GUID DefaultMaterialGUID;

        inline bool ResolveVirtualPath(
            const std::filesystem::path& inputPath,
            const std::filesystem::path& basePath,
            std::filesystem::path& outPath) const
        {
            return TransformToVirtualPath &&
                TransformToVirtualPath(UserData, inputPath, basePath, outPath);
        }

        inline bool ResolveGUID(
            const std::filesystem::path& virtualPath,
            Pitaya::Core::GUID& outGUID) const
        {
            return GetAssetGUIDByPath &&
                GetAssetGUIDByPath(UserData, virtualPath, outGUID);
        }

        inline bool RegisterFile(
            const std::filesystem::path& inputPath,
            const std::filesystem::path& basePath,
            std::filesystem::path& outVirtualPath,
            Pitaya::Core::GUID& outGUID) const
        {
            return RegisterExternalFile &&
                RegisterExternalFile(UserData, inputPath, basePath, outVirtualPath, outGUID);
        }

        inline bool RegisterGeneratedMaterial(
            const std::filesystem::path& materialPath,
            const std::filesystem::path& modelDirectory,
            const MaterialImportSource& source,
            Pitaya::Core::GUID& outGUID) const
        {
            return RegisterMaterial && RegisterMaterial(
                UserData, materialPath, modelDirectory, source, outGUID);
        }
    };
}
