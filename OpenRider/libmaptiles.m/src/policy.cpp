#include "maptiles/policy.h"

#include <algorithm>

namespace maptiles {

void DelayPolicy::on_success() { delay_ms = std::max(kDelayFloorMs, delay_ms * 0.9); }

void DelayPolicy::on_error() { delay_ms = delay_ms * 2.0 + 500.0; }

ServiceState::ServiceState(size_t servers) : servers_(servers == 0 ? 1 : servers) {}

int ServiceState::pick_server(double now_s)
{
    for (size_t i = 0; i < servers_.size(); ++i) {
        const size_t s = (next_ + i) % servers_.size();
        if (servers_[s].dropped_until <= now_s) {
            next_ = (s + 1) % servers_.size();
            return static_cast<int>(s);
        }
    }
    return -1;
}

// @impl 0f87-6acd-2f33-629b
void ServiceState::report(size_t server, bool ok, double now_s)
{
    if (server >= servers_.size())
        return;
    Server &s = servers_[server];
    if (ok) {
        delay.on_success();
        s.consecutive_failures = 0;
        s.ever_succeeded = true;
        unavailable_ = false;
        return;
    }
    delay.on_error();
    ++s.consecutive_failures;
    // Set a failing server aside only while another one is working.
    bool others_ok = false;
    for (size_t i = 0; i < servers_.size(); ++i)
        if (i != server && servers_[i].ever_succeeded && servers_[i].consecutive_failures == 0)
            others_ok = true;
    if (others_ok)
        s.dropped_until = now_s + kServerDropSeconds;
    // Unavailable when every server is failing, after at least 3 failures.
    int total = 0;
    bool all_failing = true;
    for (const Server &x : servers_) {
        total += x.consecutive_failures;
        if (x.consecutive_failures == 0)
            all_failing = false;
    }
    if (all_failing && total >= std::max<int>(3, static_cast<int>(servers_.size())))
        unavailable_ = true;
}

} // namespace maptiles
