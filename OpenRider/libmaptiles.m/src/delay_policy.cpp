#include "maptiles/delay_policy.h"

#include <algorithm>

namespace maptiles {

void DelayPolicy::OnSuccess() { delay_ms = std::max(kDelayFloorMs, delay_ms * 0.9); }

void DelayPolicy::OnError() { delay_ms = delay_ms * 2.0 + 500.0; }

} // namespace maptiles
