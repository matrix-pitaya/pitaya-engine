#pragma once

#include<Core/Allocate/Allocate.h>
#include<Core/Container/ThreadSafe/ThreadSafeQueue.h>
#include<Core/Container/ThreadSafe/ThreadSafeHashMap.h>
#include<Core/Container/ThreadSafe/ThreadSafeBidirectionalMap.h>
#include<Core/Asset/AssetRef.h>
#include<Core/Utils/Time.h>

#include<Serialize/Serializable.h>
#include<Asset/Common/FuncTable.h>
#include<Log/Common/FuncTable.h>
#include<Task/Common/FuncTable.h>
#include<GPU/Common/ShaderType.h>
#include<Import/TextureImporter/Frontend/TextureImporter.h>
#include<Import/ShaderImporter/ShaderImporter.h>
#include<Asset/Common/AssetOperate.h>
#include<Asset/Common/Material.h>
#include<Asset/Common/Mesh.h>
#include<Asset/Common/RenderTarget.h>
#include<Asset/Common/Shader.h>
#include<Asset/Common/Texture.h>
#include<Asset/Common/AssetType.h>
#include<Asset/Common/SkyBox.h>

#include<chrono>

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
        struct AssetRegistry : public Pitaya::Serialize::Serializable
        {
            void Serialize(Pitaya::Serialize::SerializeContext&) const override;
            void Deserialize(const Pitaya::Serialize::DeserializeContext&) override;

            Pitaya::Core::ThreadSafeBidirectionalMap<Pitaya::Core::GUID, std::filesystem::path> Map;
        };
        struct BuildInAsset
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
            if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>) { if (guid == Pitaya::Asset::Shader::PBRStatic) { return Pitaya::Core::AssetRef<Pitaya::Asset::Shader>(&buildIn.PBRStaticShader); } else if (guid == Pitaya::Asset::Shader::PBRSkinned) { return Pitaya::Core::AssetRef<Pitaya::Asset::Shader>(&buildIn.PBRSkinnedShader); } }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Material>) { if (guid == Pitaya::Asset::Material::PBR) { return Pitaya::Core::AssetRef<Pitaya::Asset::Material>(&buildIn.PBRMaterial); } }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>) { if (guid == Pitaya::Asset::Texture::White) { return Pitaya::Core::AssetRef<Pitaya::Asset::Texture>(&buildIn.White); } else if (guid == Pitaya::Asset::Texture::Black) { return Pitaya::Core::AssetRef<Pitaya::Asset::Texture>(&buildIn.Black); } else if (guid == Pitaya::Asset::Texture::FlatNormal) { return Pitaya::Core::AssetRef<Pitaya::Asset::Texture>(&buildIn.FlatNormal); } }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>) { if (guid == Pitaya::Asset::Mesh::Sphere) { return Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>(&buildIn.Sphere); } else if (guid == Pitaya::Asset::Mesh::Cube) { return Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>(&buildIn.Cube); } else if (guid == Pitaya::Asset::Mesh::Panel) { return Pitaya::Core::AssetRef<Pitaya::Asset::Mesh>(&buildIn.Panel); } }
            if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>) { if (guid == Pitaya::Asset::SkyBox::Default) { return Pitaya::Core::AssetRef<Pitaya::Asset::SkyBox>(&buildIn.DefaultSkyBox); } }

            std::filesystem::path path;
            if (!GetAssetPathByGUID(guid, path))
            {
                Pitaya::Log::Error(guid.ToString() + " asset cant find path");
                return nullptr;
            }
            return LoadAsset<T>(guid, path);
        }
        template<AssetType T>
        inline bool UnloadAsset(Pitaya::Core::GUID guid)
        {
            if (IsBuildInAsset<T>(guid))
            {
                Pitaya::Log::Write(Pitaya::Log::LogLevel::Warning, "Can't Unload Buildin Asset!");
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
            bool success = false;
            registry.Map.FindOperateKV(guid,
                [&success, &out_path](Pitaya::Core::GUID _guid, const std::filesystem::path& _path)
                {
                    out_path = _path;
                    success = true;
                },
                [&log](Pitaya::Core::GUID _guid)
                {
                    log = "find path fail, GUID:" + _guid.ToString();
                });
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
            bool success = false;
            registry.Map.FindOperateKV(path,
                [&success, &out_guid](const std::filesystem::path& _path, Pitaya::Core::GUID _guid)
                {
                    out_guid = _guid;
                    success = true;
                },
                [&log](const std::filesystem::path& _path)
                {
                    log = "find guid fail, path:" + _path.string();
                });
            if (!log.empty()) { Pitaya::Log::Error(log); }
            return success;
        }

    private:
        inline std::filesystem::path GetResolvePath(const std::filesystem::path& virtualPath) const
        {
            std::string virtualPathStr = virtualPath.string();
            if (virtualPathStr.starts_with("engine:/"))
            {
                return engineRoot / virtualPathStr.substr(8);
            }

            if (virtualPathStr.starts_with("project:/"))
            {
                return projectRoot / virtualPathStr.substr(9);
            }

            return projectRoot / virtualPath;
        }

    private:
        template<AssetType T>
        inline bool IsBuildInAsset(Pitaya::Core::GUID guid)
        {
            if constexpr (std::is_same_v<T, Pitaya::Asset::Texture>) { return guid == Pitaya::Asset::Texture::White || guid == Pitaya::Asset::Texture::Black || guid == Pitaya::Asset::Texture::FlatNormal; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Shader>) { return guid == Pitaya::Asset::Shader::PBRStatic || guid == Pitaya::Asset::Shader::PBRSkinned; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Mesh>) { return guid == Pitaya::Asset::Mesh::Cube || guid == Pitaya::Asset::Mesh::Panel || guid == Pitaya::Asset::Mesh::Sphere; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::Material>) { return guid == Pitaya::Asset::Material::PBR; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::RenderTarget>) { return false; }
            if constexpr (std::is_same_v<T, Pitaya::Asset::SkyBox>) { return guid == Pitaya::Asset::SkyBox::Default; }
        }
        inline bool IsBuildInAsset(const std::filesystem::path& virtualPath)
        {
            return virtualPath.string().starts_with("engine:/");
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
            assetOperateQueue.Push(operate);
        }

    public:
        inline void SyncAssetToGPU(Pitaya::Core::PassKey<Pitaya::Render::Renderer>)
        {
            // 每一帧最多处理5个资源操作 避免过多资源操作导致的卡顿
            if (cacheAssetOperateQueue.empty()) { cacheAssetOperateQueue = assetOperateQueue.PopN(5); }

            // 在时间预算内处理资产
            Pitaya::Core::InvokeWithTimeBudget(
                [this]() ->bool { return cacheAssetOperateQueue.empty(); },
                [this]() { std::visit([this](auto& result_Inner) { this->SyncAssetOperate(result_Inner); }, cacheAssetOperateQueue.front().Data); cacheAssetOperateQueue.pop(); },
                std::chrono::milliseconds(2));
        }
        inline bool IsUploadedToGPU(Pitaya::Core::PassKey<Pitaya::Render::Renderer>)
        {
            return !cacheAssetOperateQueue.empty() || !assetOperateQueue.Empty();
        }

    private:
        void SyncAssetOperate(std::monostate&);
        void SyncAssetOperate(Pitaya::Import::Texture2DImportResult&);
        void SyncAssetOperate(Pitaya::Import::TextureCubemapImportResult&);
        void SyncAssetOperate(Pitaya::Import::ShaderImportResult&);
        void SyncAssetOperate(Pitaya::Import::StaticMeshImportResult&);
        void SyncAssetOperate(Pitaya::Import::SkinnedMeshImportResult&);
        void SyncAssetOperate(Pitaya::Import::RenderTargetImportResult&);
        void SyncAssetOperate(Pitaya::Asset::Texture2DUnloadRequire&);
        void SyncAssetOperate(Pitaya::Import::SkyBoxImportResult&);

    private:
        inline static constexpr const char* fileName = "asset.cfg";

    private:
        std::filesystem::path engineRoot;
        std::filesystem::path projectRoot;
        
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

        //存储CPU操作资源结果（任务线程添加 → 渲染线程处理）
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> assetOperateQueue;
        std::queue<Pitaya::Asset::AssetOperate> cacheAssetOperateQueue;	//渲染线程处理资源操作时的临时队列 避免长时间占用锁

        Pitaya::Asset::AssetHub::AssetRegistry registry;
        Pitaya::Asset::AssetHub::BuildInAsset buildIn;
    };
}

// TODO 重构一下AssetHub 抽象出VirtualFileSystem , 现在AssetHub耦合性太高了

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
