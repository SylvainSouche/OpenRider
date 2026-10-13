/// @file
/// The controller of the application model.
#pragma once

#include "app/app_model.h"
#include "app/intents.h"

#include "core/library.h"
#include "core/overview_loader.h"

namespace app {

/// The only code changing AppModel. It owns the background services
/// (outing scan, overview loading) and the input and filter plugins, and
/// handles the model's intents.
class AppController {
public:
    /// @param model the model this controller changes; must outlive it
    explicit AppController(AppModel &model) : model_(model) {}

    /// Stops background work.
    ~AppController();

    /// Loads the activity type tables; problems are reported as errors.
    /// @param labels_json content of activity_types.json
    /// @param source correspondence table name, e.g. "garmin_connect"
    /// @param source_json content of that table
    void LoadTypes(const std::string &labels_json, const std::string &source,
                   const std::string &source_json);

    /// Sets the input plugin (from main, or a fake in tests).
    /// @param input the plugin's vtable
    /// @param handle its open handle; must stay open until Shutdown()
    /// @param name shown in the status bar
    void SetInput(const or_input_vtable *input, void *handle, std::string name);

    /// Adds a filter plugin, disabled, with its default parameters.
    void AddFilter(const or_filter_vtable *filter);

    /// Begins scanning outings.
    void Start();

    /// Moves background progress (scan, overview tracks) into the model.
    /// @param out receives follow-up intents
    void Tick(Intents &out);

    /// Handles the model's intents (see IntentKind).
    /// @param intent the intent to handle
    /// @param out receives follow-up intents
    void Handle(const Intent &intent, Intents &out);

    /// Stops background work; call before the input closes.
    void Shutdown();

private:
    /// Selects outing `index` (-1 = none) and loads its points.
    void Select(int index);

    /// Re-applies the enabled track filters and recomputes the statistics.
    void ApplyFilters();

    /// Logs an error and queues it for display.
    void Report(const std::string &error);

    AppModel &model_;                         ///< the model this controller changes
    const or_input_vtable *input_ = nullptr;  ///< input plugin, null until SetInput()
    void *handle_ = nullptr;                  ///< its open handle
    core::Library library_;                   ///< outing scan
    core::OverviewLoader overview_;           ///< overview track loading
    bool scan_reported_ = false;    ///< the end of the scan was handled
    bool tracks_reported_ = false;  ///< the end of the overview load was reported
};

} // namespace app
