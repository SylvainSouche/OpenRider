#include "app/app_controller.h"

#include "app/reports.h"

namespace app {

AppController::AppController(AppModel &model)
    : model_(model), library_(model, input_), query_(model), outing_(model, input_), segments_(model),
      comparison_(model, input_), session_(model),
      // Tick order: outings and tracks first, then what depends on them.
      domains_{&library_, &query_, &outing_, &segments_, &comparison_, &session_}
{
}

AppController::~AppController() { Shutdown(); }

void AppController::LoadTypes(const std::string &labels_json, const std::string &source, const std::string &source_json)
{
    if (labels_json.empty() || source_json.empty()) {
        ReportError(model_, "Cannot read the activity type tables");
        return;
    }
    core::Status status = model_.types.LoadLabels(labels_json);
    if (!status.Ok())
        ReportError(model_, status.error);
    status = model_.types.LoadSource(source, source_json);
    if (!status.Ok())
        ReportError(model_, status.error);
}

void AppController::SetInput(const or_input_vtable *input, void *handle, std::string name)
{
    input_.Set(input, handle);
    model_.input_name = std::move(name);
}

void AppController::AddFilter(const or_filter_vtable *filter) { outing_.AddFilter(filter); }

void AppController::AddSegmentMethod(const or_segments_vtable *method) { segments_.AddMethod(method); }

void AppController::Start() { library_.Start("garmin_connect"); }

void AppController::Tick(Intents &out)
{
    for (ModelDomain *domain : domains_)
        domain->Tick(out);
}

void AppController::Handle(const Intent &intent, Intents &out)
{
    for (ModelDomain *domain : domains_)
        if (domain->Handles(intent.kind)) {
            domain->Handle(intent, out);
            return;
        }
}

void AppController::Shutdown()
{
    // Segment builds first (they read tracks), then the readers of the input.
    segments_.Shutdown();
    for (ModelDomain *domain : domains_)
        domain->Shutdown();
}

} // namespace app
