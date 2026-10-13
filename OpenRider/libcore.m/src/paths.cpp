#include "core/paths.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sys/stat.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace core {

namespace {

std::string Home()
{
    const char *h = std::getenv("HOME");
    return h ? h : "";
}

std::string Parent(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

} // namespace

std::string ExecutableDir()
{
    char buf[PATH_MAX];
#ifdef __APPLE__
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0)
        return ".";
    char real[PATH_MAX];
    if (realpath(buf, real))
        return Parent(real);
    return Parent(buf);
#else
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0)
        return ".";
    buf[n] = '\0';
    return parent(buf);
#endif
}

std::string BundledPluginsDir() { return Parent(ExecutableDir()) + "/plugins"; }
std::string BundledShareDir() { return Parent(ExecutableDir()) + "/share/openrider"; }
std::string UserSupportDir() { return Home() + "/Library/Application Support/OpenRider"; }
std::string UserPluginsDir() { return UserSupportDir() + "/plugins"; }
std::string UserLogsDir() { return Home() + "/Library/Logs/OpenRider"; }
std::string UserCacheDir() { return Home() + "/Library/Caches/OpenRider"; }

bool MakeDirs(const std::string &path)
{
    if (path.empty())
        return false;
    std::string partial;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = path.find('/', pos + 1);
        partial = path.substr(0, pos);
        if (mkdir(partial.c_str(), 0755) != 0 && errno != EEXIST)
            return false;
    }
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace core
