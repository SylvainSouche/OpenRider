/// @file
/// Declared parameters of plugins (module-level utilities): checking the
/// values a plugin receives against what it declared.
#pragma once

#include <openrider/filter.h>

#include <string>

namespace openrider::sdk {

/// Why `values`, one per declared parameter of `params` (`count` of them),
/// are unacceptable, or "" if they are fine: missing, not finite, or
/// outside a parameter's declared bounds.
std::string ParamsProblem(const or_filter_param *params, size_t count, const double *values);

} // namespace openrider::sdk
