/// @file
/// The selected outing domain: its points, the track filters applied to
/// them, and the statistics computed from them.
#pragma once

#include "app/app_model.h"
#include "app/input_source.h"
#include "app/model_domain.h"

#include <openrider/filter.h>

namespace app {

/// Selects an outing, loads its points and keeps its filtered points and
/// statistics up to date with the track filters.
class OutingDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    /// @param input the input plugin; must outlive the domain
    OutingDomain(AppModel &model, const InputSource &input) : model_(model), input_(input) {}

    /// Adds a filter plugin, disabled, with its default parameters.
    void AddFilter(const or_filter_vtable *filter);

    /// SelectOuting, ToggleFilter, SetFilterParam, ResetFilterParams.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;

private:
    /// Selects outing `index` (-1 = none) and loads its points.
    void Select(int index);

    /// Re-applies the enabled track filters to the raw points
    /// (non-destructive) and recomputes the statistics.
    void ApplyFilters();

    AppModel &model_;           ///< the model this domain changes
    const InputSource &input_;  ///< where points come from
};

} // namespace app
