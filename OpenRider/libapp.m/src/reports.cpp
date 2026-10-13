#include "app/reports.h"

#include "core/log.h"

namespace app {

void ReportError(AppModel &model, const std::string &error)
{
    core::LogLine("error: %s", error.c_str());
    model.errors.push_back(error);
}

} // namespace app
