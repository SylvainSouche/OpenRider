// Polite fetching rules for one tile service (a provider such as
// OpenTopoMap), shared by every connection to it whatever the pool:
// - a delay between loads: 100 ms to start, doubled + 500 ms after an
//   error, reduced by 10% after a success, never below 100 ms;
// - a server (mirror) that fails while others succeed is set aside for
//   5 minutes;
// - when every server keeps failing, the service is unavailable.
// Not thread-safe: the fetcher guards it with its own mutex.
#pragma once

#include <cstddef>
#include <vector>

namespace maptiles {

constexpr double kDelayFloorMs = 100.0;
constexpr double kServerDropSeconds = 300.0;

struct DelayPolicy {
    double delay_ms = kDelayFloorMs;
    void on_success();
    void on_error();
};

class ServiceState {
public:
    explicit ServiceState(size_t servers);

    // Server to use now (round robin over servers not set aside), or -1.
    int pick_server(double now_s);
    void report(size_t server, bool ok, double now_s);
    bool unavailable() const { return unavailable_; }

    DelayPolicy delay;
    double next_allowed_s = 0.0;  // earliest start of the next load

private:
    struct Server {
        double dropped_until = 0.0;
        int consecutive_failures = 0;
        bool ever_succeeded = false;
    };
    std::vector<Server> servers_;
    size_t next_ = 0;
    bool unavailable_ = false;
};

} // namespace maptiles
