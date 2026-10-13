#include "maptiles/service_state.h"

#include <algorithm>

namespace maptiles {

ServiceState::ServiceState(size_t server_count) : servers_(server_count == 0 ? 1 : server_count) {}

int ServiceState::PickServer(double now_s)
{
    for (size_t i = 0; i < servers_.size(); ++i) {
        const size_t candidate = (next_ + i) % servers_.size();
        if (servers_[candidate].dropped_until <= now_s) {
            next_ = (candidate + 1) % servers_.size();
            return static_cast<int>(candidate);
        }
    }
    return -1;
}

// @impl 0f87-6acd-2f33-629b
void ServiceState::Report(size_t server, bool ok, double now_s)
{
    if (server >= servers_.size())
        return;
    Server &reported = servers_[server];
    if (ok) {
        delay.OnSuccess();
        reported.consecutive_failures = 0;
        reported.ever_succeeded = true;
        unavailable_ = false;
        return;
    }
    delay.OnError();
    ++reported.consecutive_failures;
    // Set a failing server aside only while another one is working.
    bool others_ok = false;
    for (size_t i = 0; i < servers_.size(); ++i)
        if (i != server && servers_[i].ever_succeeded && servers_[i].consecutive_failures == 0)
            others_ok = true;
    if (others_ok)
        reported.dropped_until = now_s + kServerDropSeconds;
    // Unavailable when every server is failing, after at least 3 failures.
    int total_failures = 0;
    bool all_failing = true;
    for (const Server &each : servers_) {
        total_failures += each.consecutive_failures;
        if (each.consecutive_failures == 0)
            all_failing = false;
    }
    if (all_failing && total_failures >= std::max<int>(3, static_cast<int>(servers_.size())))
        unavailable_ = true;
}

} // namespace maptiles
