/// @file
/// Delay between loads from one tile service.
#pragma once

namespace maptiles {

/// Shortest delay between two loads from a service (ms).
constexpr double kDelayFloorMs = 100.0;

/// Adaptive delay between loads: 100 ms to start, doubled + 500 ms after an
/// error, reduced by 10% after a success, never below kDelayFloorMs.
struct DelayPolicy {
    double delay_ms = kDelayFloorMs;  ///< current delay between two loads

    /// A load succeeded: shortens the delay by 10%, down to the floor.
    void OnSuccess();

    /// A load failed: doubles the delay and adds 500 ms.
    void OnError();
};

} // namespace maptiles
