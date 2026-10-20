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
- 当前已完成第一步：`AssetDatabase` 同时维护旧 GUID↔虚拟路径索引和 `AssetKey`/`AssetRecord` 索引，旧 `asset.cfg` 中的 `path` 字段仍可读取。
- `AssetHub::GetAssetPathByGUID`、`GetAssetGUIDByPath`、`RegisterExternalFile` 和初始化/保存流程已经通过 `AssetDatabase` 转发，调用方 API 保持不变。
- `VirtualFileSystem` 已从 `AssetHub` 抽出路径职责，统一处理 `engine:/`、`project:/`、`cache:/` 和 `cook:/` mount；当前只初始化 engine/project，cache/cook 为后续 Cooker 保留。
- `AssetHub` 仍保留原来的 `TransformToVirtualPath`、`GetResolvePath` 和 `CheckIsVirtualPath` facade，内部全部委托给 `VirtualFileSystem`。
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

当前已完成队列边界抽取：`GpuAssetBridge` 由 `AssetHub` 独占构造和管理，CPU/导入线程只调用 `Enqueue`；消费入口要求 `PassKey<Renderer>`，并在运行时校验当前线程确实是 Render 线程。Texture、Shader、Mesh、RenderTarget 和 SkyBox 的 OpenGL 资源创建、更新和销毁已经由桥接层在该入口中执行，AssetHub 的 fallback handler 目前只保留未知操作。

异步加载请求现在为每个 `AssetEntry` 分配递增的 `LoadGeneration`，并把 generation 写入 `AssetOperate`。CPU 任务完成、AssetHub 的兜底失败处理和 GpuAssetBridge 的每类上传处理器都会校验 generation；旧一轮重载结果会被丢弃，已经创建的临时 GPU 句柄会先销毁。

请求卸载也会使当前 generation 失效，因此卸载和重载之间排队的旧 CPU/GPU 操作不会重新写入资源状态。

卸载会同时清除 `GPULoading/GPULoaded` 标志。后续再次 `LoadAsset` 时，Entry 无条件进入 `PrepareReload()` 和新的 generation，不能把已经清空 GPU 句柄的 Data 当作仍然可用的已加载资源。

引擎关闭顺序现在先停止任务调度器和渲染线程，再释放 RenderPipeline、AssetHub，最后释放 RHIDevice；任务调度器在停止态拒绝新的作业，避免退出阶段又提交资产导入任务。渲染线程退出时仍然拥有有效的 OpenGL context，可以完成 GPU 资源清理。

资产提交现在通过 Render FuncTable 调用 `Renderer::WakeupAssetUpload`，由 Renderer 在自身 mutex 下记录上传唤醒请求，再调用 `cond.notify_one()`；AssetHub 和 GpuAssetBridge 不直接访问 Renderer 的 mutex/condition_variable。这样即使后台没有新的 RenderPacket，异步资产提交也能可靠唤醒渲染线程。

纹理、Shader、Mesh、Framebuffer 和 SkyBox 上传处理器已经迁移到 `GpuAssetBridge`：Texture2D、Cubemap、Texture2D 卸载、VF/VFG Shader、Static/Skinned Mesh、RenderTarget FrameBuffer 以及 SkyBox IBL 纹理都在桥接层的 RenderThread 分派中执行，AssetHub 只提供 typed Entry 查找回调。

热重载时 Texture2D、Cubemap 和 Shader 会先在 RenderThread 创建新对象，再替换 Entry 中的句柄并销毁旧对象；Mesh、RenderTarget 和 SkyBox 使用同样的替换策略。Entry 的生命周期锁保护 GPU 句柄快照和提交，避免游戏线程卸载与 RenderThread 上传同时读写句柄。

运行时卸载现在要求没有存活的外部 `AssetRef`，并拒绝在 CPU/GPU 加载过程中卸载。`AssetHub::UnloadAsset<T>` 先递增 generation 使旧上传结果失效，再把 GPU 句柄快照提交给 `GpuAssetBridge`；卸载队列和上传队列分离，GPU 卸载命令至少延迟一个渲染循环，在上一帧 `WaitFence` 后由 RenderThread 调用对应的 OpenGL `Destroy*`。Texture（2D、2DArray、Cubemap）、Shader、Mesh（VAO/VBO/IBO）、RenderTarget（四个 FrameBuffer）和 SkyBox（Equirect/三组 Cubemap）均使用 typed unload command，避免卸载期间再通过已删除的 Entry 查找句柄。

卸载提交时还会清理 Mesh 的材质引用和 CPU 几何缓存，以及 Material 的 Shader、Texture 引用和参数数组；Entry/Data 容器保留用于重载，依赖 AssetRef 会立即释放，避免一次卸载把整条依赖链永久钉在内存中。

这次 Mesh 迁移同时修复了三个加载问题：Skinned Mesh 不再在 CPU 阶段静默返回；VAO/VBO/IBO 任一创建或链接失败时会回滚已经创建的句柄；重复加载时会先清空旧的材质引用，再根据导入结果重新绑定材质。Mesh 现在同时保存 VAO、VBO、IBO 三个句柄，重载时可以成组回收旧 GPU 缓冲。

RenderTarget 和 SkyBox 也使用同一套 RenderThread 上传路径：FrameBuffer 组或 IBL 纹理创建不完整时会清理已创建句柄，SkyBox 的 IBL 烘焙失败或收到卸载请求时会清理临时纹理，只有完整上传成功后才替换旧资源。

OpenGL 的限制决定了这一步必须保留明确的线程边界：导入任务和 CPU 资源解析可以在线程池执行，但 OpenGL 对象的创建、更新和销毁只能由拥有当前 GL context 的渲染线程执行。目标流程是：

```text
工作线程: 读取源文件 -> Import -> CPU 结果 + AssetKey/generation
              |
              v
AssetOperate 队列（只传递 CPU 数据和 GPU 命令参数）
              |
渲染线程: GpuAssetBridge -> OpenGL buffer/texture/program 创建或销毁
```

`GpuAssetBridge` 不负责找文件或解析导入格式，只把已经完成 CPU 阶段的结果转换成渲染线程可执行的 OpenGL 操作，并在操作完成后更新资源状态。这样不会把 OpenGL context 带进导入线程，也不会让 AssetDatabase 依赖 GPU 后端。

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

Material 反序列化现在会恢复 Shader 参数布局和 `Textures` 子节点中的纹理 GUID，Mesh 材质加载后可以重新建立纹理依赖。

## 7. 后续必须修复的问题

- `AssetHub::Release` 现在先抽离所有 Entry、清空 Data，再按引用计数脱离 Entry；仍被外部 `AssetRef` 持有的 Entry 会保留空 Data，并在最后一个引用释放时自动回收。
- 内置资产 Entry 现在也是独立的堆控制块。无外部引用时由 `BuiltinAssetStorage` 释放；仍被引用时从 AssetHub 脱离并保留空 Data，最后一个引用释放后自动回收，避免内嵌成员随 AssetHub 销毁后让内置 `AssetRef` 悬空。
- 运行时卸载队列目前只保证 GPU 句柄延后一帧销毁，Entry 本身仍保留在 map 中，下一阶段增加可回收 Entry 的扫描和重载复用策略。
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

