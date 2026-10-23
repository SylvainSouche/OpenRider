/// @file
/// The open input plugins and which one each outing comes from.
// @impl 0f87-6ad7-26b7-83a2
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/result.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace core {

/// The input sources open at once (GarminDB, a GPX folder...), and the
/// source of every outing listed, so that its points are read where it
/// came from. Sources are added before use; the rest is thread-safe.
class InputSet {
public:
    /// One open input plugin.
    struct Source {
        const or_input_vtable *vtable = nullptr;  ///< the plugin
        void *handle = nullptr;                   ///< its open handle
        std::string name;                         ///< the plugin's name, e.g. "garmindb"
        std::string types;                        ///< the type table its source types resolve with, e.g. "garmin_connect"
        bool confirm_types = false;               ///< its outings' types are to be confirmed by the user
        std::string import_dir;                   ///< folder its outings are files in (imports go there); "" if none
    };

    /// Adds an open source; the earlier sources win over the later ones
    /// for duplicate outings.
    /// @return its index
    size_t Add(Source source);

    /// The sources, in the order added.
    const std::vector<Source> &Sources() const { return sources_; }

    /// No source.
    bool Empty() const { return sources_.empty(); }

    /// Records that outing `id` comes from source `source`.
    void Assign(const std::string &id, size_t source);

    /// The source of outing `id`; the first one when unknown.
    size_t SourceOf(const std::string &id) const;

    /// The recorded points of outing `id`, read through its source.
    openrider::sdk::Status LoadPoints(const std::string &id, std::vector<or_point> &points) const;

    /// Closes every source (they are left without handles).
    void CloseAll();

private:
    std::vector<Source> sources_;          ///< in the order added
    mutable std::mutex mutex_;             ///< guards owner_
    std::map<std::string, size_t> owner_;  ///< outing id -> source
};

} // namespace core
