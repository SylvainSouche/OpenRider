/// @file
/// The controller of the application model.
#pragma once

#include "app/app_model.h"
#include "app/comparison_domain.h"
#include "app/input_source.h"
#include "app/intents.h"
#include "app/library_domain.h"
#include "app/outing_domain.h"
#include "app/query_domain.h"
#include "app/segments_domain.h"
#include "app/session_domain.h"

#include <openrider/filter.h>
#include <openrider/segments.h>

#include <string>
#include <vector>

namespace app {

// @impl 0f87-6ace-5fd7-0165
/// The only owner of the code changing AppModel. The model's behaviour is
/// split into domains (ModelDomain): the outing library, the list query,
/// the selected outing, segments, the segment comparison and the session.
/// AppController owns them and the input plugin they share, configures
/// them at start-up, routes every model intent to the domain handling its
/// kind, and ticks and shuts them down together.
class AppController {
public:
    /// @param model the model the domains change; must outlive the controller
    explicit AppController(AppModel &model);

    /// Stops background work.
    ~AppController();

    /// Loads the activity type tables; problems are reported as errors.
    /// @param labels_json content of activity_types.json
    /// @param source correspondence table name, e.g. "garmin_connect"
    /// @param source_json content of that table
    void LoadTypes(const std::string &labels_json, const std::string &source, const std::string &source_json);

    /// Sets the input plugin (from main, or a fake in tests).
    /// @param input the plugin's vtable
    /// @param handle its open handle; must stay open until Shutdown()
    /// @param name shown in the status bar
    void SetInput(const or_input_vtable *input, void *handle, std::string name);

    /// Adds a filter plugin, disabled, with its default parameters.
    void AddFilter(const or_filter_vtable *filter);

    /// Adds a segments plugin with its default parameters; the first one
    /// added is selected.
    void AddSegmentMethod(const or_segments_vtable *method);

    /// Begins scanning outings.
    void Start();

    /// Moves every domain's background progress into the model.
    /// @param out receives follow-up intents
    void Tick(Intents &out);

    /// Hands a model intent to the domain handling its kind (see
    /// IntentKind); ignores others.
    /// @param intent the intent to handle
    /// @param out receives follow-up intents
    void Handle(const Intent &intent, Intents &out);

    /// Stops every domain's background work; call before the input closes.
    void Shutdown();

private:
    AppModel &model_;              ///< the model the domains change
    InputSource input_;            ///< the input plugin, shared by the domains reading outings
    LibraryDomain library_;        ///< outings and overview tracks
    QueryDomain query_;            ///< which outings are listed
    OutingDomain outing_;          ///< the selected outing
    SegmentsDomain segments_;      ///< segment networks
    ComparisonDomain comparison_;  ///< the segment comparison
    SessionDomain session_;        ///< errors and quitting
    std::vector<ModelDomain *> domains_;  ///< all of them, in tick order
};

} // namespace app
