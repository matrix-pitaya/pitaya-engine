#pragma once

#include<array>
#include<cstdint>
#include<filesystem>
#include<string_view>

namespace Pitaya::Asset
{
    class VirtualFileSystem final
    {
    public:
        enum class MountId : uint8_t
        {
            Engine,
            Project,
            Cache,
            Cook
        };

    private:
        struct Mount
        {
            std::string_view Prefix;
            std::filesystem::path Root;
        };

    public:
        VirtualFileSystem() = default;
        ~VirtualFileSystem() = default;

        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
        VirtualFileSystem(VirtualFileSystem&&) = delete;
        VirtualFileSystem& operator=(VirtualFileSystem&&) = delete;

    public:
        bool Initialize(const std::filesystem::path& engineRoot, const std::filesystem::path& projectRoot,
            const std::filesystem::path& cacheRoot = {}, const std::filesystem::path& cookRoot = {});

        bool ToVirtualPath(const std::filesystem::path& inputPath, const std::filesystem::path& basePath,
            std::filesystem::path& outVirtualPath) const;
        bool Resolve(const std::filesystem::path& virtualPath, std::filesystem::path& outPhysicalPath) const;
        bool IsVirtualPath(const std::filesystem::path& path) const;
        bool IsEnginePath(const std::filesystem::path& path) const;

        const std::filesystem::path& GetRoot(MountId mountId) const noexcept;

    private:
        static std::filesystem::path NormalizeRoot(const std::filesystem::path& root);
        static std::filesystem::path NormalizePath(const std::filesystem::path& path, const std::filesystem::path& basePath);
        static bool IsPathInRoot(const std::filesystem::path& path, const std::filesystem::path& root) noexcept;
        static bool IsSafeRelativePath(const std::filesystem::path& path) noexcept;
        static size_t GetMountIndex(MountId mountId) noexcept;

    private:
        std::array<Mount, 4> mounts{
            Mount{ "engine:/", {} },
            Mount{ "project:/", {} },
            Mount{ "cache:/", {} },
            Mount{ "cook:/", {} }
        };
    };
}
