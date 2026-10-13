/// @file
/// The outing library domain: outings and overview tracks read in the
/// background, and the status bar's account of it.
#pragma once

#include "app/app_model.h"
#include "app/input_source.h"
#include "app/model_domain.h"

#include "core/library.h"
#include "core/overview_loader.h"

#include <string>

namespace app {

/// Reads every outing of the input (Library), then every track for the
/// overview (OverviewLoader), moving them into the model as they come.
class LibraryDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    /// @param input the input plugin; must outlive the domain
    LibraryDomain(AppModel &model, const InputSource &input) : model_(model), input_(input) {}

    /// Begins scanning outings; their types resolved with the model's
    /// catalog through the `source` correspondence table.
    void Start(const std::string &source);

    /// Handles no intent: the library only follows its background work.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;
    void Tick(Intents &out) override;
    void Shutdown() override;

private:
    AppModel &model_;                ///< the model this domain changes
    const InputSource &input_;       ///< where outings come from
    core::Library library_;          ///< outing scan
    core::OverviewLoader overview_;  ///< overview track loading
    bool scan_reported_ = false;     ///< the end of the scan was handled
    bool tracks_reported_ = false;   ///< the end of the overview load was reported
};

} // namespace app
