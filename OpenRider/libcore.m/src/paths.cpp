#include "core/paths.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sys/stat.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <direct.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#endif

namespace core {

namespace {

#ifndef _WIN32
std::string Home()
{
    const char *h = std::getenv("HOME");
    return h ? h : "";
}

#ifndef __APPLE__
/// An XDG base directory: `variable` when set to an absolute path (the
/// specification ignores relative ones), else `~/<fallback>`.
std::string XdgDir(const char *variable, const char *fallback)
{
    const char *value = std::getenv(variable);
    if (value && value[0] == '/')
        return value;
    return Home() + "/" + fallback;
}
#endif
#else
/// The value of environment variable `name`, with '/' separators; empty when unset.
std::string EnvDir(const char *name)
{
    const char *value = std::getenv(name);
    std::string dir = value ? value : "";
    for (char &c : dir)
        if (c == '\\')
            c = '/';
    return dir;
}
#endif

std::string Parent(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

} // namespace

std::string ExecutableDir()
{
#ifdef __APPLE__
    char buf[PATH_MAX];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0)
        return ".";
    char real[PATH_MAX];
    if (realpath(buf, real))
        return Parent(real);
    return Parent(buf);
#elif defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buf, sizeof buf);
    if (n == 0 || n == sizeof buf)
        return ".";
    std::string path(buf, n);
    for (char &c : path)
        if (c == '\\')
            c = '/';
    return Parent(path);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    // No /proc by default: the kernel's record of the process's path.
#if defined(__FreeBSD__)
    int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
#else
    int name[] = {CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME};
#endif
    char buf[PATH_MAX];
    size_t size = sizeof buf;
    if (sysctl(name, 4, buf, &size, nullptr, 0) != 0 || size == 0)
        return ".";
    return Parent(buf);
#else
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0)
        return ".";
    buf[n] = '\0';
    return Parent(buf);
#endif
}

std::string BundledPluginsDir() { return Parent(ExecutableDir()) + "/plugins"; }
std::string BundledShareDir() { return Parent(ExecutableDir()) + "/share/openrider"; }
#ifdef _WIN32
// @impl 1c06-6ad1-21c2-3a5c
std::string UserSupportDir() { return EnvDir("APPDATA") + "/OpenRider"; }
std::string UserPluginsDir() { return UserSupportDir() + "/plugins"; }
std::string UserLogsDir() { return EnvDir("LOCALAPPDATA") + "/OpenRider/Logs"; }
std::string UserCacheDir() { return EnvDir("LOCALAPPDATA") + "/OpenRider/Cache"; }
#elif defined(__APPLE__)
std::string UserSupportDir() { return Home() + "/Library/Application Support/OpenRider"; }
std::string UserPluginsDir() { return UserSupportDir() + "/plugins"; }
std::string UserLogsDir() { return Home() + "/Library/Logs/OpenRider"; }
std::string UserCacheDir() { return Home() + "/Library/Caches/OpenRider"; }
#else
// @impl 0f87-6adb-701e-a77c
std::string UserSupportDir() { return XdgDir("XDG_DATA_HOME", ".local/share") + "/OpenRider"; }
std::string UserPluginsDir() { return UserSupportDir() + "/plugins"; }
std::string UserLogsDir() { return XdgDir("XDG_STATE_HOME", ".local/state") + "/OpenRider/logs"; }
std::string UserCacheDir() { return XdgDir("XDG_CACHE_HOME", ".cache") + "/OpenRider"; }
#endif

bool MakeDirs(const std::string &path)
{
    if (path.empty())
        return false;
    std::string partial;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = path.find('/', pos + 1);
        partial = path.substr(0, pos);
#ifdef _WIN32
        if (partial.back() == ':')
            continue;  // a drive ("C:"), not a directory to create
        if (_mkdir(partial.c_str()) != 0 && errno != EEXIST)
#else
        if (mkdir(partial.c_str(), 0755) != 0 && errno != EEXIST)
#endif
            return false;
    }
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace core
