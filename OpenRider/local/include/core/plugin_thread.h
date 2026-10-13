/// @file
/// Calls into plugins and their threads (docs/plugin-api.md 5.1). A plugin
/// declaring OR_PLUGIN_THREAD_SAFE is called from whatever thread needs
/// it; every call into any other plugin goes through one thread of its own,
/// so that such a plugin never sees two threads, nor a change of thread.
// @impl 0f87-6ace-72ef-00b1
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace core {

/// One thread running calls into a plugin, one at a time, in order.
class PluginThread {
public:
    PluginThread();
    /// Stops the thread once the calls already queued are done.
    ~PluginThread();
    PluginThread(const PluginThread &) = delete;
    PluginThread &operator=(const PluginThread &) = delete;

    /// Runs `call` on this thread and returns once it is done; runs it at
    /// once when already on this thread (a call made from within a call).
    void Run(const std::function<void()> &call);

private:
    /// The thread's loop: runs queued calls until stopped.
    void Loop();

    std::mutex mutex_;                                  ///< guards queue_ and stop_
    std::condition_variable wake_;                      ///< a call is queued, or stop
    std::deque<std::function<void()>> queue_;           ///< calls not yet run
    bool stop_ = false;                                 ///< set by the destructor
    std::thread thread_;                                ///< the thread itself
};

/// Declares the plugin owning function table `vtable` as not thread-safe:
/// from now on CallPlugin() runs its calls on a PluginThread of its own.
/// Done by LoadPlugins() for each plugin without OR_PLUGIN_THREAD_SAFE.
void SerialisePlugin(const void *vtable);

/// Runs `call`, a call into the plugin owning function table `vtable`, and
/// returns once it is done: at once for a thread-safe plugin, on the
/// plugin's own thread otherwise. Thread-safe.
void CallPlugin(const void *vtable, const std::function<void()> &call);

} // namespace core
