/// @file
/// The segment comparison domain: the passages of the outings along a
/// segment.
#pragma once

#include "app/app_model.h"
#include "app/input_source.h"
#include "app/model_domain.h"

namespace app {

/// Opens and closes AppModel::comparison: on opening, every passage of the
/// segment's outings along it, from their recorded points.
class ComparisonDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    /// @param input the input plugin; must outlive the domain
    ComparisonDomain(AppModel &model, const InputSource &input) : model_(model), input_(input) {}

    /// OpenSegmentComparison, CloseSegmentComparison.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;

private:
    /// Opens the comparison of segment `segment` of group `group`.
    void Open(int group, int segment);

    AppModel &model_;           ///< the model this domain changes
    const InputSource &input_;  ///< where points come from
};

} // namespace app
