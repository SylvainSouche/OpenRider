/// @file
/// Background loading of every outing's track for the overview map.
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/overview.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace core {

/// Reads the points of a list of outings through the input plugin on a
/// worker thread and thins them into overview tracks.
class OverviewLoader {
public:
    /// Stops a running load.
    ~OverviewLoader();

    /// Starts loading the tracks of `ids`.
    /// @param input the input plugin's vtable
    /// @param handle the plugin's open handle; must stay open until Stop()
    /// @param ids outings to load, in loading order
    void Start(const or_input_vtable *input, void *handle, std::vector<std::string> ids);

    /// A load is running.
    bool Loading() const { return loading_.load(); }

    /// Outings processed so far (loaded or failed).
    size_t Done() const { return done_.load(); }

    /// Outings to process in the current load.
    size_t Total() const { return total_.load(); }

    /// Appends the tracks read so far to `out`.
    /// @return how many were appended
    size_t Take(std::vector<openrider::sdk::OverviewTrack> &out);

    /// Interrupts a running load and waits for it; call before closing the input.
    void Stop();

private:
    /// The load, run on the worker thread.
    void Run(const or_input_vtable *input, void *handle, std::vector<std::string> ids);

    std::thread worker_;                ///< runs Run()
    std::atomic<bool> loading_{false};  ///< a load is running
    std::atomic<bool> stop_{false};     ///< asks the worker to stop
    std::atomic<size_t> done_{0};       ///< outings processed
    std::atomic<size_t> total_{0};      ///< outings to process
    std::mutex mutex_;                     ///< guards pending_
    std::vector<openrider::sdk::OverviewTrack> pending_;   ///< read, not yet taken
};

} // namespace core
