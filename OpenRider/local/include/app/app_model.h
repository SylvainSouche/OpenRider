/// @file
/// The application model: everything the app knows, independent of how it
/// is shown.
#pragma once

#include "core/library.h"
#include "core/outing_selection.h"
#include "core/overview.h"
#include "core/stats.h"
#include "core/type_catalog.h"

#include <openrider/filter.h>
#include <openrider/input.h>

#include <set>
#include <string>
#include <vector>

namespace app {

/// Which outings are listed (and drawn on the overview map).
struct OutingQuery {
    std::string type_key;               ///< ActivityType key, "" = all types
    std::string from_date;              ///< "YYYY-MM-DD", "" = open
    std::string to_date;                ///< "YYYY-MM-DD", inclusive, "" = open
    bool only_visible = false;          ///< list restricted to tracks visible on the overview
    std::set<std::string> visible_ids;  ///< maintained from the overview map's extent
};

/// A filter plugin and its settings.
struct FilterState {
    const or_filter_vtable *vtable = nullptr;  ///< the filter plugin
    bool enabled = false;                      ///< applied to the selected outing
    std::vector<double> values;  ///< one per declared parameter
};

/// The application model. Only AppController changes it; view controllers
/// and views read it.
struct AppModel {
    core::TypeCatalog types;  ///< activity type labels and correspondence tables
    std::string lang = "en";  ///< display language, "en" or "fr"

    /// @name Outings, read in the background through the input plugin
    /// @{
    std::string input_name;             ///< input plugin, for the status bar
    std::vector<core::Outing> outings;  ///< read so far
    bool scanning = false;              ///< the scan is running
    size_t scanned = 0;                 ///< outings read so far
    size_t scan_total = 0;              ///< outings announced; 0 while unknown
    /// @}

    /// @name Every track, thinned, for the overview
    /// @{
    std::vector<core::OverviewTrack> tracks;  ///< loaded so far
    bool tracks_loading = false;              ///< the load is running
    size_t tracks_done = 0;                   ///< outings processed so far
    size_t tracks_total = 0;                  ///< outings to process
    /// @}

    OutingQuery query;  ///< which outings are listed
    int selected = -1;  ///< index into outings, -1 = none

    /// @name The selected outing
    /// Points as recorded, after the enabled track filters, and statistics
    /// computed from them.
    /// @{
    std::string points_id;             ///< outing the points belong to
    std::vector<or_point> raw_points;  ///< as recorded
    std::vector<or_point> points;      ///< after the enabled track filters
    core::ComputedStats computed;      ///< statistics computed from them
    std::vector<FilterState> filters;  ///< every filter plugin, in load order
    /// @}

    std::vector<std::string> errors;  ///< oldest first, shown one at a time
    std::string status;               ///< what is going on, for the status bar
    bool quit = false;                ///< the application is to end

    /// At least one filter is enabled.
    bool AnyFilterOn() const;

    /// Filter of the outing list: type, dates and visibility on the overview.
    core::OutingFilter ListFilter() const;

    /// Filter of the overview map: type and dates only.
    core::OutingFilter MapFilter() const;

    /// The selected outing, or null.
    const core::Outing *SelectedOuting() const;
};

} // namespace app
