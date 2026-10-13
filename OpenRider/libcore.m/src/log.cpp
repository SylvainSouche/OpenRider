#include "core/log.h"

#include "core/paths.h"

#include <cstdarg>
#include <cstdio>
#include <dirent.h>
#include <mutex>

namespace core {

namespace {

std::mutex g_mutex;
FILE *g_file = nullptr;
std::string g_path;

constexpr const char *kPrefix = "OpenRider-";
constexpr const char *kSuffix = ".log";

std::vector<std::string> ListDir(const std::string &dir)
{
    std::vector<std::string> names;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d))
            names.push_back(e->d_name);
        closedir(d);
    }
    return names;
}

} // namespace

std::string LogFileName(std::time_t when)
{
    std::tm tm{};
    localtime_r(&when, &tm);
    char buf[64];
    std::strftime(buf, sizeof buf, "OpenRider-%Y-%m-%d_%H-%M-%S.log", &tm);
    return buf;
}

std::time_t LogFileTime(const std::string &name)
{
    std::tm tm{};
    char tail[8] = "";
    if (name.rfind(kPrefix, 0) != 0 ||
        std::sscanf(name.c_str() + 10, "%4d-%2d-%2d_%2d-%2d-%2d%4s", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, tail) != 7 ||
        std::string(tail) != kSuffix)
        return -1;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

std::vector<std::string> ExpiredLogs(const std::vector<std::string> &names, std::time_t now,
                                      int days)
{
    std::vector<std::string> out;
    const std::time_t limit = now - static_cast<std::time_t>(days) * 86400;
    for (const std::string &n : names) {
        const std::time_t t = LogFileTime(n);
        if (t >= 0 && t < limit)
            out.push_back(n);
    }
    return out;
}

// @impl 0f87-6acd-24ce-0aa3
bool LogOpenSession(const std::string &dir)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file)
        return true;
    if (!MakeDirs(dir))
        return false;
    const std::time_t now = std::time(nullptr);
    for (const std::string &old : ExpiredLogs(ListDir(dir), now, kLogRetentionDays))
        std::remove((dir + "/" + old).c_str());
    g_path = dir + "/" + LogFileName(now);
    g_file = std::fopen(g_path.c_str(), "a");
    return g_file != nullptr;
}

void LogLine(const char *format, ...)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file)
        return;
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%H:%M:%S ", &tm);
    std::fputs(stamp, g_file);
    va_list ap;
    va_start(ap, format);
    std::vfprintf(g_file, format, ap);
    va_end(ap);
    std::fputc('\n', g_file);
    std::fflush(g_file);
}

std::string LogSessionPath()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_path;
}

} // namespace core
