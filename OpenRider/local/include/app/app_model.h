/// @file
/// The application model: everything the app knows, independent of how it
/// is shown.
#pragma once

#include "core/library.h"
#include "core/network_builder.h"
#include "core/outing_selection.h"
#include "core/type_catalog.h"

#include <openrider/filter.h>
#include <openrider/input.h>
#include <openrider/sdk/overview.h>
#include <openrider/sdk/stats.h>
#include <openrider/segments.h>

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

/// A segments plugin and its settings.
struct SegmentMethod {
    const or_segments_vtable *vtable = nullptr;  ///< the plugin
    std::vector<double> values;                  ///< one per declared parameter
};

/// One passage of an outing along the compared segment.
struct ComparedRun {
    size_t outing = 0;             ///< index into AppModel::outings
    int pass = 1;                  ///< 1, 2... when the outing travels the segment several times
    bool forward = true;           ///< travelled in the segment's direction
    std::vector<or_point> points;  ///< its points along the segment; time and distance from the entry
    std::vector<double> along_m;   ///< position along the segment of each point
    double duration_s = 0.0;       ///< time from entry to exit
};

/// The segment being compared, and every passage of the outings along it.
struct SegmentComparison {
    bool open = false;               ///< the comparison panel is shown
    unsigned generation = 0;         ///< changes with every comparison opened
    std::string sport;               ///< activity group of the segment
    openrider::sdk::NetworkSegment segment;    ///< a copy: networks are rebuilt
    std::vector<ComparedRun> runs;   ///< newest outing first
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
    std::vector<openrider::sdk::OverviewTrack> tracks;  ///< loaded so far
    bool tracks_loading = false;              ///< the load is running
    size_t tracks_done = 0;                   ///< outings processed so far
    size_t tracks_total = 0;                  ///< outings to process
    /// @}

    /// @name Common segments of the listed outings, one network per sport
    /// @{
    bool show_segments = false;                     ///< the overview shows them
    std::vector<SegmentMethod> segment_methods;     ///< every segments plugin, in load order
    int segment_method = -1;                        ///< the one used, -1 = none
    std::vector<core::SegmentGroup> segment_groups;  ///< last networks built
    bool segments_building = false;                 ///< a build is running
    /// @}

    SegmentComparison comparison;  ///< the segment comparison panel's content

    OutingQuery query;  ///< which outings are listed
    int selected = -1;  ///< index into outings, -1 = none

    /// @name The selected outing
    /// Points as recorded, after the enabled track filters, and statistics
    /// computed from them.
    /// @{
    std::string points_id;             ///< outing the points belong to
    std::vector<or_point> raw_points;  ///< as recorded
    std::vector<or_point> points;      ///< after the enabled track filters
    openrider::sdk::ComputedStats computed;      ///< statistics computed from them
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
