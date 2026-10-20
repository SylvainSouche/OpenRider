/// @file
/// The outing library domain: outings and overview tracks read in the
/// background, and the status bar's account of it.
#pragma once

#include "app/app_model.h"
#include "app/input_source.h"
#include "app/model_domain.h"

#include "core/library.h"
#include "core/outing_types.h"
#include "core/overview_loader.h"

#include <set>
#include <string>

namespace app {

/// Reads every outing of the input (Library), then every track for the
/// overview (OverviewLoader, through the enabled track filters), moving them
/// into the model as they come.
class LibraryDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    /// @param input the input plugin; must outlive the domain
    LibraryDomain(AppModel &model, InputSource &input) : model_(model), input_(input) {}

    /// Begins scanning the outings of every input source; their types
    /// resolved with the model's catalog, each source with its own table.
    void Start();

    /// Reads the types the user settled for outings from `path` (and saves
    /// them there); they override the sources' types.
    void SetTypesFile(const std::string &path);

    /// ReloadOverview: the overview tracks loaded again, with the filters
    /// now enabled. SetOutingType, ConfirmOutingTypes: outing types chosen
    /// and settled by the user (remembered in the types file). ImportFiles,
    /// ImportAccept, ImportCancel: files imported one after the other --
    /// each copied into the folder of the source taking imports, read back
    /// as an outing, then kept (its type settled) or dropped.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;
    void Tick(Intents &out) override;
    void Shutdown() override;

private:
    /// (Re)starts loading every outing's overview track.
    void LoadOverview();

    /// Takes the next file of the import queue: copies it, reads it back
    /// and makes it the outing asked about; refused files are reported.
    /// The overview is reloaded once the queue is done.
    void NextImport();

    AppModel &model_;                ///< the model this domain changes
    InputSource &input_;             ///< where outings come from
    core::OutingTypes types_;        ///< types settled by the user
    std::set<std::string> hidden_ids_;  ///< outings already left out as duplicates
    std::string import_copy_;        ///< the copy of the file being imported
    bool imported_ = false;          ///< files kept since the overview was loaded
    core::Library library_;          ///< outing scan
    core::OverviewLoader overview_;  ///< overview track loading
    bool scan_reported_ = false;     ///< the end of the scan was handled
    bool tracks_reported_ = false;   ///< the end of the overview load was reported
};

} // namespace app
