# Pitaya 资产系统重构计划

## 1. 文档目的

本文记录 Pitaya 资产系统从当前实现迁移到可扩展运行时资产管线的设计、边界和分阶段落地计划。目标是让编辑器导入、运行时加载、GPU 上传和发行版打包互相隔离，同时保留现有 `Asset::LoadAsset<T>(GUID)` API 作为兼容层。

## 2. 当前入口和调用链

当前程序入口为：

```text
Application/main.cpp
  -> Application::Execute
  -> Engine::Execute
  -> Engine::Initialize
  -> TaskScheduler / AssetHub / GameWorld / RHIDevice / Renderer
  -> 主循环 BeginFrame -> Update -> Render -> EndFrame
```

资产加载链路为：

```text
Game / Editor
  -> Asset::LoadAsset<T>(GUID)
  -> FuncTable<AssetHub>
  -> AssetHub::LoadAsset<T>
  -> TaskScheduler::PostJob
  -> Import::Import
  -> AssetOperate queue
  -> Renderer render thread
  -> AssetHub::SyncAssetToGPU
  -> GPU resources
```

当前 `AssetHub` 同时承担以下职责：

- GUID 与虚拟路径注册、持久化和解析
- 内置资产存储和初始化
- 资产类型判断和扩展名探测
- CPU 导入任务调度
- AssetEntry 状态和引用计数
- CPU 到 GPU 的操作队列
- 渲染线程上的 GPU 创建和销毁
- Engine FuncTable 的跨模块桥接

这使得 AssetHub 成为单体对象。任何导入格式、GPU 后端或生命周期规则变化都会修改 AssetHub。

## 3. Import 库当前问题

Import 已经是独立的 Visual Studio 静态库，但源代码边界仍然是环形的：

```text
Asset -> Import -> Asset
```

具体表现：

1. `Import/Common/ImportResult.h` 直接依赖 `Asset::Mesh`、`Asset::TextureUsage` 和 GPU 类型。
2. `AssimpMeshImporter.cpp` 直接调用 `Asset::TransformToVirtualPath`、`GetAssetGUIDByPath` 和 `RegisterExternalFile`。
3. Assimp 导入器会创建 `.mat` 文件并修改 `asset.cfg`，把导入器和 AssetDatabase 绑定在一起。
4. Assimp 导入器构造假的 `AssetEntry` 和 `AssetRef` 来复用材质逻辑。
5. `Import/Import.h` 使用 Engine 的导出宏和调用约定，底层导入库反向依赖 Engine ABI。
6. Import 的错误通过全局 Log FuncTable 输出，没有结构化诊断、取消和导入上下文。

Import 应该只负责把源文件转换为中立的 CPU 导入数据，不应访问 AssetRuntime、AssetDatabase、GPU 或渲染线程。

## 4. 目标架构

```text
Core
  └── ImportData / AssetKey / Hash

Import
  ├── TextureImporter
  ├── ShaderImporter
  └── MeshImporter

Asset
  ├── VirtualFileSystem
  ├── AssetDatabase
  ├── ImportPipeline
  ├── RuntimeAssetManager
  ├── GpuAssetBridge
  ├── BuiltinAssetRegistry
  └── AssetCooker

Render / Game
  └── AssetHandle / AssetView

Engine
  └── 负责模块生命周期和服务注册
```

### 4.1 稳定身份

资产身份使用 GUID 和类型组成的 `AssetKey`，路径只属于元数据：

```cpp
struct AssetKey
{
    Core::GUID Id;
    AssetTypeId Type;
};
```

### 4.2 AssetRecord

Catalog 至少记录：

- `AssetKey`
- source virtual path
- cooked/imported artifact path
- asset origin
- importer id 和版本
- source hash、导入设置 hash
- 依赖列表
- 构建和运行时 flags
- 最近一次导入错误

### 4.3 生命周期

资源状态需要覆盖：

```text
Unloaded -> Queued -> LoadingCPU -> WaitingDependencies
         -> UploadingGPU -> Ready
         -> Failed / Cancelled / Stale
```

所有异步操作携带 `generation`。旧一轮加载结果不能覆盖新一轮资源。

### 4.4 发行版

编辑器和 Cooker 执行：

```text
Source -> Import -> Dependency Graph -> Cook Artifact -> Build Manifest
```

Release 运行时只读取 Manifest 和 Cook Artifact，不扫描源文件，也不加载 Assimp、StbImage 等导入器。

## 5. 分阶段迁移

### 阶段一：切断 Import 到 Asset 的反向调用

- 增加 `ImportContext`，由上层提供路径和资产注册回调。
- Assimp 通过上下文请求依赖解析，不再直接调用 AssetHub 的路径、GUID 和材质序列化 API。
- Assimp 只生成 `MaterialImportSource`，材质文件生成和纹理 GUID 注册由 AssetHub 回调完成。
- 保留旧 `Import::Import` 重载，内部转发到带上下文的新接口。
- 暂时保持 MeshImportResult 中的 GPU/Asset 数据结构，保证运行时行为和构建稳定；下一阶段再替换为 Import 自有的中立 MeshData。

### 阶段二：拆 AssetDatabase 和 VirtualFileSystem

- 将 `AssetHub::AssetRegistry`、虚拟路径转换、路径解析和外部文件注册移动到 `AssetDatabase`。
- 将 `engine:/`、`project:/`、`cache:/` 和 `cook:/` mount 统一放进 VirtualFileSystem。
- 将 `asset.cfg` 升级为带版本号的 Catalog。

### 阶段三：拆 ImportPipeline 和 RuntimeAssetManager

- AssetHub 只保留 facade。
- 导入任务、Entry map、状态和引用交给 RuntimeAssetManager。
- 各类导入函数移动到 ImportPipeline。
- 增加依赖图、循环检测和结构化错误。

### 阶段四：拆 GpuAssetBridge

- `AssetOperate` 改为带 `AssetKey + generation` 的上传命令。
- Renderer 只负责消费 GPU 上传队列。
- 删除 AssetHub 对 `PassKey<Renderer>` 的直接依赖。
- 增加 GPU fence 和延迟销毁。

### 阶段五：Cook 和编辑器工具

- 增加 BuildManifest、artifact hash、依赖闭包和 chunk 分组。
- Editor 提供资产状态、依赖图、缺失引用、重复 GUID 和循环依赖诊断。
- Release 构建移除源文件导入路径。

## 6. 当前阶段验收标准

- Assimp 导入器不再构造 `AssetEntry`、`AssetRef` 或直接写 Asset 文件。
- Import 不再直接调用 Asset 的路径、GUID 或序列化 API。
- AssetHub 仍能通过旧的 `LoadAsset<T>(GUID)` API 加载现有资源。
- Release x64 构建通过。
- 现有内置资产、纹理、Shader、Mesh、Material 和 SkyBox 加载行为不改变。

## 7. 后续必须修复的问题

- `AssetHub::Release` 直接删除裸 `AssetEntry*`。
- `UnloadAsset` 不检查外部引用计数。
- AssetOperate 结果没有 generation。
- Material 反序列化没有恢复 Textures。
- Skinned Mesh 导入失败时可能长期停留在 CPULoading。
- StbImage 的全局 flip 状态不适合并行导入。
- HDR 图片检测后仍可能使用 8-bit 导入路径。
- Release 运行时仍然执行源文件导入。

## 8. Visual Studio 构建约束

所有 C++ 项目必须从仓库根目录的 `Pitaya.sln` 构建。禁止直接对单个 `.vcxproj` 执行构建，因为 Visual Studio 在这种情况下会把 `$(SolutionDir)` 解析成项目目录，导致 `intermediate` 和 `library` 出现在项目文件夹内部。

仓库根目录的 `Directory.Build.targets` 已加入强制检查：如果 `$(SolutionDir)Pitaya.sln` 不存在，任何 C++ 项目构建都会直接失败。`Import/Import.vcxproj` 的输出路径同时改为基于项目文件计算仓库根目录，固定写入：

```text
<workspace>/intermediate/<Project>/<Configuration>/
<workspace>/library/<Platform>/<Configuration>/
```

