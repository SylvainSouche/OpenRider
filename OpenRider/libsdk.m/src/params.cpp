#include <openrider/sdk/params.h>

#include <cmath>

namespace openrider::sdk {

std::string ParamsProblem(const or_filter_param *params, size_t count, const double *values)
{
    if (count > 0 && (!params || !values))
        return "missing parameter values";
    for (size_t p = 0; p < count; ++p)
        if (!std::isfinite(values[p]) || values[p] < params[p].min_value || values[p] > params[p].max_value)
            return std::string("parameter ") + params[p].id + " out of its bounds";
    return "";
}

} // namespace openrider::sdk
