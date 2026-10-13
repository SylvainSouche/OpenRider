/// @file
/// The input plugin the model's outings come from.
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/result.h>

#include <string>
#include <vector>

namespace app {

/// The input plugin and its open handle, shared by the domains reading
/// outings (library, selected outing, comparison).
class InputSource {
public:
    /// Sets the plugin (from main, or a fake in tests).
    /// @param vtable the plugin's vtable
    /// @param handle its open handle; must stay open while the source is used
    void Set(const or_input_vtable *vtable, void *handle);

    /// An input plugin is set.
    bool Ready() const { return vtable_ != nullptr; }

    /// The plugin's vtable, null until Set().
    const or_input_vtable *Vtable() const { return vtable_; }

    /// The plugin's open handle.
    void *Handle() const { return handle_; }

    /// Loads every recorded point of outing `id` into `points` (replacing
    /// them).
    /// @return the plugin's error, if any
    openrider::sdk::Status LoadPoints(const std::string &id, std::vector<or_point> &points) const;

private:
    const or_input_vtable *vtable_ = nullptr;  ///< the plugin
    void *handle_ = nullptr;                   ///< its open handle
};

} // namespace app
