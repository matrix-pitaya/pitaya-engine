#include<Asset/VirtualFileSystem/VirtualFileSystem.h>

#include<algorithm>
#include<string>

namespace
{
    bool StartsWith(std::string_view value, std::string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }
}

bool Pitaya::Asset::VirtualFileSystem::Initialize(
    const std::filesystem::path& engineRoot, const std::filesystem::path& projectRoot,
    const std::filesystem::path& cacheRoot, const std::filesystem::path& cookRoot)
{
    mounts[GetMountIndex(MountId::Engine)].Root = NormalizeRoot(engineRoot);
    mounts[GetMountIndex(MountId::Project)].Root = NormalizeRoot(projectRoot);
    mounts[GetMountIndex(MountId::Cache)].Root = NormalizeRoot(cacheRoot);
    mounts[GetMountIndex(MountId::Cook)].Root = NormalizeRoot(cookRoot);
    return !mounts[GetMountIndex(MountId::Engine)].Root.empty()
        && !mounts[GetMountIndex(MountId::Project)].Root.empty();
}

bool Pitaya::Asset::VirtualFileSystem::ToVirtualPath(
    const std::filesystem::path& inputPath, const std::filesystem::path& basePath,
    std::filesystem::path& outVirtualPath) const
{
    std::filesystem::path resolvedBasePath = basePath;
    if (!basePath.empty() && IsVirtualPath(basePath) && !Resolve(basePath, resolvedBasePath))
    {
        return false;
    }

    std::filesystem::path resolvedInputPath = inputPath;
    if (IsVirtualPath(inputPath) && !Resolve(inputPath, resolvedInputPath))
    {
        return false;
    }
    const std::filesystem::path absolutePath = NormalizePath(resolvedInputPath, resolvedBasePath);
    for (const Mount& mount : mounts)
    {
        if (mount.Root.empty() || !IsPathInRoot(absolutePath, mount.Root)) { continue; }

        const std::filesystem::path relativePath = absolutePath.lexically_relative(mount.Root);
        const std::string relativeString = relativePath.generic_string();
        outVirtualPath = std::filesystem::path(
            std::string(mount.Prefix) + (relativeString.empty() ? std::string() : "/" + relativeString));
        return true;
    }
    return false;
}

bool Pitaya::Asset::VirtualFileSystem::Resolve(
    const std::filesystem::path& virtualPath, std::filesystem::path& outPhysicalPath) const
{
    const std::string virtualPathString = virtualPath.generic_string();
    for (const Mount& mount : mounts)
    {
        if (mount.Root.empty() || !StartsWith(virtualPathString, mount.Prefix)) { continue; }

        std::string_view relativeString = std::string_view(virtualPathString).substr(mount.Prefix.size());
        if (!relativeString.empty() && relativeString.front() == '/') { relativeString.remove_prefix(1); }
        const std::filesystem::path relativePath(relativeString);
        if (!IsSafeRelativePath(relativePath)) { return false; }

        outPhysicalPath = (mount.Root / relativePath).lexically_normal();
        return true;
    }
    return false;
}

bool Pitaya::Asset::VirtualFileSystem::IsVirtualPath(const std::filesystem::path& path) const
{
    const std::string pathString = path.generic_string();
    return std::any_of(mounts.begin(), mounts.end(), [&pathString](const Mount& mount)
        {
            return !mount.Root.empty() && StartsWith(pathString, mount.Prefix);
        });
}

bool Pitaya::Asset::VirtualFileSystem::IsEnginePath(const std::filesystem::path& path) const
{
    const Mount& engineMount = mounts[GetMountIndex(MountId::Engine)];
    return !engineMount.Root.empty() && StartsWith(path.generic_string(), engineMount.Prefix);
}

const std::filesystem::path& Pitaya::Asset::VirtualFileSystem::GetRoot(MountId mountId) const noexcept
{
    return mounts[GetMountIndex(mountId)].Root;
}

std::filesystem::path Pitaya::Asset::VirtualFileSystem::NormalizeRoot(const std::filesystem::path& root)
{
    if (root.empty()) { return {}; }
    try
    {
        return std::filesystem::weakly_canonical(root);
    }
    catch (...)
    {
        try { return std::filesystem::absolute(root).lexically_normal(); }
        catch (...) { return root.lexically_normal(); }
    }
}

std::filesystem::path Pitaya::Asset::VirtualFileSystem::NormalizePath(
    const std::filesystem::path& path, const std::filesystem::path& basePath)
{
    std::filesystem::path absolutePath = path.is_absolute() ? path : basePath / path;
    try
    {
        return std::filesystem::canonical(absolutePath);
    }
    catch (...)
    {
        try
        {
            return std::filesystem::weakly_canonical(absolutePath);
        }
        catch (...)
        {
            try { return std::filesystem::absolute(absolutePath).lexically_normal(); }
            catch (...) { return absolutePath.lexically_normal(); }
        }
    }
}

bool Pitaya::Asset::VirtualFileSystem::IsPathInRoot(
    const std::filesystem::path& path, const std::filesystem::path& root) noexcept
{
    if (path.root_name() != root.root_name()) { return false; }
    const std::filesystem::path relativePath = path.lexically_relative(root);
    if (relativePath.empty()) { return true; }
    for (const auto& component : relativePath)
    {
        if (component == "..") { return false; }
    }
    return true;
}

bool Pitaya::Asset::VirtualFileSystem::IsSafeRelativePath(const std::filesystem::path& path) noexcept
{
    if (path.is_absolute()) { return false; }
    for (const auto& component : path)
    {
        if (component == "..") { return false; }
    }
    return true;
}

size_t Pitaya::Asset::VirtualFileSystem::GetMountIndex(MountId mountId) noexcept
{
    return static_cast<size_t>(mountId);
}
