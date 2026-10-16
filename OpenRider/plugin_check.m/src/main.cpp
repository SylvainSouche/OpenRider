// openrider-plugin-check -- the published plugin test tool
// (docs/plugin-api.md §9). Loads each plugin given on the command line
// through libcore's own loader, core::LoadPlugin(), so it accepts exactly
// what the application accepts, and reports every check in TAP. Exit
// status: 0 when every plugin is accepted, 1 when one is refused, 2 on a
// usage error.

#include "core/plugins.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

const char kUsage[] =
    "usage: openrider-plugin-check [--kind KIND] PLUGIN...\n"
    "  Loads each PLUGIN library the way OpenRider does and reports in TAP.\n"
    "  KIND is input, filters, tiles, workflows or segments; without it, it is\n"
    "  the name of the directory holding the plugin (plugins/<kind>/).\n";

/// File name part of `path`, either separator.
std::string BaseName(const std::string &path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// Name of the directory holding `path`, either separator; "" if none.
std::string ParentName(const std::string &path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "" : BaseName(path.substr(0, slash));
}

/// Writes TAP test points, numbering them.
class Tap {
public:
    void Ok(const std::string &what) { std::printf("ok %d - %s\n", ++count_, what.c_str()); }
    void NotOk(const std::string &what, const std::string &why)
    {
        std::printf("not ok %d - %s\n# %s\n", ++count_, what.c_str(), why.c_str());
        failed_ = true;
    }
    void Skip(const std::string &what) { std::printf("ok %d - %s # SKIP\n", ++count_, what.c_str()); }
    void Plan() const { std::printf("1..%d\n", count_); }
    bool Failed() const { return failed_; }

private:
    int count_ = 0;
    bool failed_ = false;
};

/// Reports the three checks the application's loader makes on `path`.
void Check(Tap &tap, const std::string &path, uint32_t kind)
{
    const std::string name = BaseName(path);
    const std::string open = name + ": library opens";
    const std::string entry = name + ": exports " OR_PLUGIN_ENTRY_SYMBOL;
    const std::string descriptor = name + ": descriptor accepted as a '" + core::PluginKindDir(kind) + "' plugin";

    const core::PluginLoad load = core::LoadPlugin(path, kind);
    switch (load.stage) {
    case core::PluginStage::kOpen:
        tap.NotOk(open, load.problem);
        tap.Skip(entry);
        tap.Skip(descriptor);
        return;
    case core::PluginStage::kEntry:
        tap.Ok(open);
        tap.NotOk(entry, load.problem);
        tap.Skip(descriptor);
        return;
    case core::PluginStage::kDescriptor:
        tap.Ok(open);
        tap.Ok(entry);
        tap.NotOk(descriptor, load.problem);
        return;
    case core::PluginStage::kAccepted: break;
    }
    tap.Ok(open);
    tap.Ok(entry);
    tap.Ok(descriptor);
    const or_plugin_info &info = *load.plugin.info;
    std::printf("# %s %s, ABI %u.%u%s\n", info.name, info.version ? info.version : "", info.api_version,
                info.api_minor, (info.flags & OR_PLUGIN_THREAD_SAFE) ? ", thread-safe" : "");
}

} // namespace

// @impl 1c06-6ad1-23a6-133d
int main(int argc, char **argv)
{
#ifdef _WIN32
    // TAP lines end in \n on every platform, never \r\n.
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    uint32_t kind = 0;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            std::fputs(kUsage, stdout);
            return 0;
        }
        if (std::strcmp(argv[i], "--kind") == 0 || std::strcmp(argv[i], "-k") == 0) {
            if (++i == argc || !(kind = core::PluginKindFromDir(argv[i]))) {
                std::fprintf(stderr, "openrider-plugin-check: --kind needs one of input, filters, tiles, "
                                     "workflows, segments\n%s",
                             kUsage);
                return 2;
            }
            continue;
        }
        paths.push_back(argv[i]);
    }
    if (paths.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }

    std::printf("# OpenRider plugin ABI %u.%u\n", OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR);
    Tap tap;
    for (const std::string &path : paths) {
        const uint32_t k = kind ? kind : core::PluginKindFromDir(ParentName(path));
        if (!k) {
            tap.NotOk(BaseName(path) + ": plugin kind known",
                      "not in a plugins/<kind>/ directory; give --kind");
            continue;
        }
        Check(tap, path, k);
    }
    tap.Plan();
    return tap.Failed() ? 1 : 0;
}
