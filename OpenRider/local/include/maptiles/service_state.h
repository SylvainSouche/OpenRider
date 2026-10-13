/// @file
/// Polite fetching rules for one tile service (a provider such as
/// OpenTopoMap), shared by every connection to it whatever the pool.
#pragma once

#include "maptiles/delay_policy.h"

#include <cstddef>
#include <vector>

namespace maptiles {

/// How long a failing server (mirror) is set aside (s).
constexpr double kServerDropSeconds = 300.0;

/// State of one tile service across its servers (mirrors):
/// - one shared delay between loads (see DelayPolicy);
/// - a server that fails while others succeed is set aside for
///   kServerDropSeconds;
/// - when every server keeps failing, the service is unavailable.
///
/// Not thread-safe: the fetcher guards it with its own mutex.
class ServiceState {
public:
    /// @param server_count number of servers (mirrors) of the service; 0 counts as 1
    explicit ServiceState(size_t server_count);

    /// Server to use now: round robin over the servers not set aside.
    /// @param now_s current time (s, monotonic)
    /// @return server index, or -1 when every server is set aside
    int PickServer(double now_s);

    /// Records the outcome of a load from `server` and updates the delay,
    /// the set-aside servers and availability.
    /// @param server index returned by PickServer()
    /// @param ok the load succeeded (a "no tile here" answer counts as success)
    /// @param now_s current time (s, monotonic)
    void Report(size_t server, bool ok, double now_s);

    /// Every server keeps failing.
    bool Unavailable() const { return unavailable_; }

    DelayPolicy delay;            ///< delay between loads, whatever the server
    double next_allowed_s = 0.0;  ///< earliest start of the next load (s, monotonic)

private:
    /// Health of one server.
    struct Server {
        double dropped_until = 0.0;    ///< set aside until this time (s)
        int consecutive_failures = 0;  ///< failures since the last success
        bool ever_succeeded = false;   ///< has served at least one load
    };
    std::vector<Server> servers_;  ///< one per mirror
    size_t next_ = 0;            ///< round-robin cursor
    bool unavailable_ = false;   ///< every server keeps failing
};

} // namespace maptiles
