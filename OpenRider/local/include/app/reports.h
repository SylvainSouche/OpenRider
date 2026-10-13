/// @file
/// Reporting errors to the user (module-level utilities).
#pragma once

#include "app/app_model.h"

#include <string>

namespace app {

/// Logs `error` and queues it in the model for display.
void ReportError(AppModel &model, const std::string &error);

} // namespace app
