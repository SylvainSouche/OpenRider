/// @file
/// The segments domain: the segment identification methods (plugins) and
/// the networks of the listed outings.
#pragma once

#include "app/app_model.h"
#include "app/input_source.h"
#include "app/model_domain.h"

#include "core/network_builder.h"

#include <openrider/segments.h>

#include <string>

namespace app {

/// Keeps AppModel::segment_groups following the overview: while common
/// segments are shown, the outings passing the map filter are grouped by
/// sport and their networks built in the background with the selected
/// method, from the outings' recorded points; a change of filter, outings,
/// method or parameter starts a new build, the previous networks staying
/// shown until it finishes.
class SegmentsDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    /// @param input the input plugin, read by the builds; must outlive the domain
    SegmentsDomain(AppModel &model, const InputSource &input) : model_(model), input_(input) {}

    /// Adds a segments plugin with its default parameters; the first one
    /// added is selected.
    void AddMethod(const or_segments_vtable *method);

    /// SetShowSegments, SelectSegmentMethod, SetSegmentParam.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;
    void Tick(Intents &out) override;
    void Shutdown() override;

private:
    /// Starts a build when what the networks depend on changed, and takes a
    /// finished one.
    void Update();

    AppModel &model_;               ///< the model this domain changes
    const InputSource &input_;      ///< the input plugin
    core::NetworkBuilder builder_;  ///< background builds
    std::string basis_;             ///< what the networks were built from
};

} // namespace app
