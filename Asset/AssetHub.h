#pragma once

#include<Core/Allocate/Allocate.h>
#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Asset/AssetRef.h>

#include<Asset/Common/FuncTable.h>
#include<Asset/Database/AssetDatabase.h>
#include<Log/Common/FuncTable.h>
#include<Task/Common/FuncTable.h>
#include<GPU/Common/ShaderType.h>
#include<Import/TextureImporter/Frontend/TextureImporter.h>
#include<Import/ShaderImporter/ShaderImporter.h>
#include<Import/Common/ImportContext.h>
#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/Material.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/RenderTarget.h>
#include<Asset/Common/Shader.h>
#include<Asset/Common/Texture.h>
#include<Asset/Common/AssetType.h>
#include<Asset/Common/AssetKey.h>
#include<Asset/Common/AssetCatalog.h>
#include<Asset/Common/SkyBox.h>
#include<Asset/GpuAssetBridge/GpuAssetBridge.h>
#include<Asset/VirtualFileSystem/VirtualFileSystem.h>

namespace Pitaya::Asset
{
    class AssetHub
    {
    public:
        struct Factory
        {
            friend class Pitaya::Engine::Module<AssetHub>;
        private:
            static AssetHub* Create()
            {
                return PITAYA_NEW(AssetHub);
            }
            static void Destroy(AssetHub* assetHub)
            {
                PITAYA_DELETE(assetHub);
            }
        };
        struct Lifecycle
        {
            friend class Pitaya::Engine::Module<AssetHub>;
        private:
            static bool Initialize(AssetHub* assetHub)
            {
                return assetHub->Initialize();
            }
            static void Release(AssetHub* assetHub)
            {
                assetHub->Release();
            }
        };

    private:
        struct BuiltinAssetStorage
        {
            Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry White;
            Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry Black;
            Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry FlatNormal;
            Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry PBRStaticShader;
            Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry PBRSkinnedShader;
            Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry PBRMaterial;
            Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry Cube;
            Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry Panel;
            Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry Sphere;
            Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry DefaultSkyBox;
        };

    private:
        AssetHub() = default;
        ~AssetHub() = default;

    public:
        AssetHub(const AssetHub&) = delete;
        AssetHub& operator=(const AssetHub&) = delete;
        AssetHub(AssetHub&&) = delete;
        AssetHub& operator=(AssetHub&&) = delete;

    private:
        bool Initialize();
        void Release();

    public:
        template<AssetType T>
        inline Pitaya::Core::AssetRef<T> LoadAsset(Pitaya::Core::GUID guid)
        {
            Pitaya::Asset::BuiltinAssetInfo builtinInfo;
            if (builtinCatalog.Find(guid, builtinInfo))
            {
                if (builtinInfo.Key.Type != Pitaya::Asset::AssetTraits<T>::Type)
                {
                    Pitaya::Log::Error("asset type mismatch for builtin GUID:" + guid.ToString());
                    return nullptr;
                }
                return FindAssetEntry<T>(guid);
            }

            std::filesystem::path path;
            if (!GetAssetPathByGUID(guid, path))
            {
                Pitaya::Log::Error(guid.ToString() + " asset cant find path");
                return nullptr;
            }
            if (!database.RegisterAssetType(guid, Pitaya::Asset::AssetTraits<T>::Type))
            {
                Pitaya::Log::Error("asset type mismatch for GUID:" + guid.ToString());
                return nullptr;
            }
            return LoadAsset<T>(guid, path);
        }
        template<AssetType T>
        inline bool UnloadAsset(Pitaya::Core::GUID guid)
        {
            if (builtinCatalog.Matches<T>(guid))
            {
                Pitaya::Log::Write(Pitaya::Log::LogLevel::Warning, "Can't unload builtin asset!");
                return false;
            }

            std::string log;
            Pitaya::Log::LogLevel level = Pitaya::Log::LogLevel::Info;
            bool success = false;
            auto& map = GetAssetEntryMap<T>();
            map.FindOperateKV(guid,
                [&success, &log, &level](Pitaya::Core::GUID _guid, typename Pitaya::Core::AssetRef<T>::AssetEntry* _entry)
                {
                    if (!_entry)
                    {
                        level = Pitaya::Log::LogLevel::Error;
                        log = "asset entry is empty GUID:" + _guid.ToString();
                        success = false;
                        return;
                    }

                    _entry->RequestUnload();
                    level = Pitaya::Log::LogLevel::Info;
                    log = "asset unload requested GUID:" + _guid.ToString();
                    success = true;
                },
                [&success, &log, &level](Pitaya::Core::GUID _guid)
                {
                    level = Pitaya::Log::LogLevel::Error;
                    log = "cant find asset entry GUID:" + _guid.ToString();
                    success = false;
                });
            Pitaya::Log::Write(level, log);
            return success;
        }

    private:
        static Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry* FindTextureEntryCallback(
            void* userData, Pitaya::Core::GUID guid);

        template<AssetType T>
        inline Pitaya::Core::AssetRef<T> FindAssetEntry(Pitaya::Core::GUID guid)
        {
            Pitaya::Core::AssetRef<T> asset = nullptr;
            GetAssetEntryMap<T>().FindOperateKV(guid,
                [&asset](Pitaya::Core::GUID, typename Pitaya::Core::AssetRef<T>::AssetEntry* entry)
                {
                    if (entry) { asset = entry; }
                },
                [](Pitaya::Core::GUID) {});
            if (!asset)
            {
                Pitaya::Log::Error("builtin asset is registered but not initialized GUID:" + guid.ToString());
            }
            return asset;
        }

        void RegisterBuiltinAssets();
        Pitaya::Import::ImportContext GetImportContext() noexcept;

    private:
        template<AssetType T>
        inline Pitaya::Core::AssetRef<T> LoadAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path)
        {
            if (!CheckIsVirtualPath(path))
            {
                Pitaya::Log::Error("load asset fail! path is not virtual path, path:" + path.string());
                return nullptr;
            }

            std::filesystem::path resolvePath = GetResolvePath(path);
            if (!CheckAssetValid(resolvePath))
            {
                Pitaya::Log::Error("load asset fail! path invalid, path:" + resolvePath.string());
                return nullptr;
            }

            std::string log;
            Pitaya::Log::LogLevel level = Pitaya::Log::LogLevel::Info;
            bool asyncLoad = false;
            typename Pitaya::Core::AssetRef<T> asset = nullptr;
            auto& map = GetAssetEntryMap<T>();
            map.FindOperateKVOrInsert(guid,
                [&asset, &asyncLoad, &log, &level](Pitaya::Core::GUID _guid, typename Pitaya::Core::AssetRef<T>::AssetEntry* _entry)
                {
                    if (!_entry)
                    {
                        log = "asset entry is empty! GUID:" + _guid.ToString();
                        level = Pitaya::Log::LogLevel::Error;
                        return;
                    }

                    log = "asset entry reused GUID:" + _guid.ToString();
                    level = Pitaya::Log::LogLevel::Info;
                    asset = _entry;

                    if (_entry->IsUnloadRequested() && _entry->HasLoadedData())
                    {
                        _entry->State.ClearBits(Pitaya::Core::AssetState::Unload);
                    }
                    else if (_entry->IsUnloadRequested() || _entry->IsFailed())
                    {
                        _entry->PrepareReload();
                        asyncLoad = true;
                    }
                },
                [&asset, &asyncLoad, &log, &level](Pitaya::Core::GUID _guid) -> std::optional<std::pair<Pitaya::Core::GUID, typename Pitaya::Core::AssetRef<T>::AssetEntry*>>
                {
                    level = Pitaya::Log::LogLevel::Info;
                    log = "asset entry inexistence, create asset entry GUID:" + _guid.ToString();

                    asyncLoad = true;
                    typename Pitaya::Core::AssetRef<T>::AssetEntry* entry = Pitaya::Core::New<typename Pitaya::Core::AssetRef<T>::AssetEntry>();
                    entry->GUID = _guid;
                    entry->Data.store(Pitaya::Core::New<T>(), std::memory_order_release);
                    entry->BeginCpuLoad();
                    asset = entry;
                    return std::make_pair(_guid, entry);
                });
            Pitaya::Log::Write(level, log);
            if (asyncLoad) { AsyncLoadAsset<T>(guid, resolvePath); }
            return asset;
        }

    private:
        template<AssetType T>
        inline void AsyncLoadAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path)
        {
            Pitaya::Task::PostJob(
                [this, guid, path]()
                {
                    if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>) { LoadTextureAsset(guid, path); }
                    else if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>) { LoadShaderAsset(guid, path); }
                    else if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>) { LoadMeshAsset(guid, path); }
                    else if constexpr (std::is_same_v<T, Pitaya::Asset::Material>) { LoadMaterialAsset(guid, path); }
                    else if constexpr (std::is_same_v<T, Pitaya::Asset::RenderTarget>) { LoadRenderTarget(guid, path); }
                    else if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>) { LoadSkyBoxAsset(guid, path); }
                    else { Pitaya::Log::Error("async load asset fail,GUID:" + guid.ToString() + " path:" + path.string()); }

                    typename Pitaya::Core::AssetRef<T>::AssetEntry* entry = nullptr;
                    GetAssetEntryMap<T>().FindOperateKV(guid,
                        [&entry](Pitaya::Core::GUID, typename Pitaya::Core::AssetRef<T>::AssetEntry* value) { entry = value; },
                        [](Pitaya::Core::GUID) {});
                    if (entry && entry->State.HasBits(Pitaya::Core::AssetState::CPULoading))
                    {
                        entry->FailCpuLoad();
                        Pitaya::Log::Error("asset CPU load did not complete GUID:" + guid.ToString());
                    }
                }, "Async Load Asset path: " + path.string() + " GUID: " + guid.ToString());
        }

    public:
        bool TransformToVirtualPath(const std::filesystem::path& inputPath, const std::filesystem::path& basePath, std::filesystem::path& out_virtualpath) const;
        bool RegisterExternalFile(const std::filesystem::path& inputPath, const std::filesystem::path& basePath, std::filesystem::path& out_virtualpath, Pitaya::Core::GUID& out_guid);
        bool RegisterGeneratedMaterial(const std::filesystem::path& materialPath, const std::filesystem::path& modelDirectory,
            const Pitaya::Import::MaterialImportSource& source, Pitaya::Core::GUID& out_guid);

    private:
        template<AssetType T>
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            typename Pitaya::Core::AssetRef<T>::AssetEntry*>& GetAssetEntryMap()
        {
            if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>) { return textures; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>) { return shaders; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>) { return meshes; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Material>) { return materials; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::RenderTarget>) { return rendertargets; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>) { return skyboxes; }
        }

    public:
        inline bool GetAssetPathByGUID(Pitaya::Core::GUID guid, std::filesystem::path& out_path)
        {
            std::string log;
            const bool success = database.GetAssetPathByGUID(guid, out_path);
            if (!success) { log = "find path fail, GUID:" + guid.ToString(); }
            if (!log.empty()) { Pitaya::Log::Error(log); }
            return success;
        }
        inline bool GetAssetGUIDByPath(const std::filesystem::path& path, Pitaya::Core::GUID& out_guid)
        {
            if (!CheckIsVirtualPath(path))
            {
                Pitaya::Log::Debug("get guid by path fail, path not virtual, path:" + path.string());
                return false;
            }

            std::string log;
            const bool success = database.GetAssetGUIDByPath(path, out_guid);
            if (!success) { log = "find guid fail, path:" + path.string(); }
            if (!log.empty()) { Pitaya::Log::Error(log); }
            return success;
        }

    private:
        inline std::filesystem::path GetResolvePath(const std::filesystem::path& virtualPath) const
        {
            std::filesystem::path physicalPath;
            virtualFileSystem.Resolve(virtualPath, physicalPath);
            return physicalPath;
        }

    private:
        inline bool IsEngineAssetPath(const std::filesystem::path& virtualPath)
        {
            return virtualFileSystem.IsEnginePath(virtualPath);
        }

    //private:
    //	Pitaya::Asset::AssetType GetAssetType(const std::filesystem::path& path) const;

    private:
        //TODO 实现函数 每一帧遍历16-32个资源，判断能否销毁
        //TODO 实现函数 强制遍历所有资源，判断能否销毁

    private:
        void LoadTextureAsset(Core::GUID guid, const std::filesystem::path& path);
        bool LoadTexture2DAsset(Core::GUID guid, const std::filesystem::path& path, Pitaya::Import::Texture2DImportResult& result);
        bool LoadTextureCubemapAsset(Core::GUID guid, const std::filesystem::path& path, const std::vector<std::filesystem::path>& paths, Pitaya::Import::TextureCubemapImportResult& result);

    private:
        void LoadShaderAsset(Core::GUID guid, const std::filesystem::path& folder);
        bool LoadVFShaderAsset(Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
            const std::filesystem::path& fragmentPath, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner);
        bool LoadVFGShaderAsset(Core::GUID guid, const std::filesystem::path& folder, const std::filesystem::path& vertexPath,
            const std::filesystem::path& fragmentPath, const std::filesystem::path& geometry, Pitaya::Import::ShaderImportResult& cpuOpResult_Inner);

    private:
        void LoadMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file);
        bool LoadStaticMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& detectResult, Pitaya::Import::StaticMeshImportResult& out);
        bool LoadSkinnedMeshAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file, const Pitaya::Import::MeshPreloadResult& detectResult, Pitaya::Import::SkinnedMeshImportResult& out);

    private:
        void LoadMaterialAsset(Pitaya::Core::GUID guid, const std::filesystem::path& file);

    private:
        void LoadRenderTarget(Pitaya::Core::GUID guid, const std::filesystem::path& file);

    private:
        void LoadSkyBoxAsset(Pitaya::Core::GUID guid, const std::filesystem::path& path);

    private:
        bool CheckIsVirtualPath(const std::filesystem::path& path) const;
        bool CheckAssetValid(const std::filesystem::path& path) const;

    private:
        bool CheckIsValidTexture2DFile(const std::filesystem::path& file) const;
        bool CheckIsValidTextureCubeMapFolder(const std::filesystem::path& folder, std::vector<std::filesystem::path>& out) const;
        bool CheckIsValidShaderFloder(const std::filesystem::path& folder, std::unordered_map<Pitaya::GPU::ShaderType, std::filesystem::path>& out) const;
        bool CheckIsValidMaterialFile(const std::filesystem::path&) const;
        bool CheckIsValidRenderTargetFile(const std::filesystem::path&) const;
        bool CheckIsValidMeshFile(const std::filesystem::path& file) const;

    public:
        inline void CommitAssetOperate(const Pitaya::Asset::AssetOperate& operate)
        {
            gpuAssetBridge.Enqueue(operate);
        }
        inline void CommitAssetOperate(Pitaya::Asset::AssetOperate&& operate)
        {
            gpuAssetBridge.Enqueue(std::move(operate));
        }

    public:
        inline void SyncAssetToGPU(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey)
        {
            gpuAssetBridge.Process(passkey,
                [this](auto& result_Inner) { this->SyncAssetOperate(result_Inner); });
        }
        inline bool IsUploadedToGPU(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey)
        {
            return gpuAssetBridge.HasPending(passkey);
        }

    private:
        void SyncAssetOperate(std::monostate&);
        void SyncAssetOperate(Pitaya::Import::ShaderImportResult&);
        void SyncAssetOperate(Pitaya::Import::StaticMeshImportResult&);
        void SyncAssetOperate(Pitaya::Import::SkinnedMeshImportResult&);
        void SyncAssetOperate(Pitaya::Import::RenderTargetImportResult&);
        void SyncAssetOperate(Pitaya::Import::SkyBoxImportResult&);

    private:
        inline static constexpr const char* fileName = "asset.cfg";

    private:
        Pitaya::Asset::VirtualFileSystem virtualFileSystem;
        
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>::AssetEntry*> meshes;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Texture>::AssetEntry*> textures;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Shader>::AssetEntry*> shaders;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::Material>::AssetEntry*> materials;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::RenderTarget>::AssetEntry*> rendertargets;
        Pitaya::Core::ThreadSafeHashMap<Pitaya::Core::GUID,
            Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>::AssetEntry*> skyboxes;

        // CPU 线程提交，Renderer 线程消费；OpenGL 操作只在 GpuAssetBridge::Process 中执行。
        Pitaya::Asset::GpuAssetBridge gpuAssetBridge;

        Pitaya::Asset::AssetDatabase database;
        Pitaya::Asset::AssetHub::BuiltinAssetStorage builtins;
        Pitaya::Asset::BuiltinAssetCatalog builtinCatalog;
    };
}

// AssetHub 保留资源生命周期和导入 facade，虚拟路径职责由 VirtualFileSystem 处理。

/*
bool CanUnloadAsset(AssetEntry* entry) {
    auto state = entry->State.load(std::memory_order_acquire);
        return (entry->RefCount == 0)          // 无任何引用
        && (state & AssetState::Unload)    // 标记了Unload
        && !(state & AssetState::CPULoading) // 无CPU加载中
        && !(state & AssetState::GPULoading) // 无GPU加载中
        && !(state & AssetState::CPULoading) // 重复？不，是确认无加载中
        // 补充：如果已生成GPU数据，需先提交删除指令，再删CPU数据
        && ( (state & AssetState::GPULoaded) ? SubmitGPUUnloadCmd(entry) : true );
}
*/
