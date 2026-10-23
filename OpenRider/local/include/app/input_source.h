/// @file
/// The input sources the model's outings come from.
#pragma once

#include "core/input_set.h"

#include <openrider/input.h>
#include <openrider/sdk/result.h>

#include <string>
#include <vector>

namespace app {

/// The open input sources (GarminDB, a GPX folder...), shared by the
/// domains reading outings (library, selected outing, segments,
/// comparison).
class InputSource {
public:
    /// Adds an open source (from main, or a fake in tests); the earlier
    /// ones win over the later ones for duplicate outings.
    void Add(core::InputSet::Source source) { set_.Add(std::move(source)); }

    /// At least one source is open.
    bool Ready() const { return !set_.Empty(); }

    /// The sources, and which one each outing comes from.
    core::InputSet &Set() { return set_; }

    /// The sources, read-only.
    const core::InputSet &Set() const { return set_; }

    /// Loads every recorded point of outing `id` into `points` (replacing
    /// them), from the outing's source.
    /// @return the plugin's error, if any
    openrider::sdk::Status LoadPoints(const std::string &id, std::vector<or_point> &points) const
    {
        return set_.LoadPoints(id, points);
    }

private:
    core::InputSet set_;  ///< the sources
};

} // namespace app
