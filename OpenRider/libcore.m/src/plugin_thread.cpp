#include "core/plugin_thread.h"

#include <map>
#include <memory>

namespace core {

namespace {

/// The threads of the plugins that are not thread-safe, by function table.
struct Registry {
    std::mutex mutex;                                         ///< guards threads
    std::map<const void *, std::unique_ptr<PluginThread>> threads;  ///< by vtable
};

Registry &TheRegistry()
{
    // Never destroyed: plugins stay loaded for the process lifetime, and a
    // background task may still call one while static objects go.
    static Registry *registry = new Registry;
    return *registry;
}

} // namespace

PluginThread::PluginThread() : thread_(&PluginThread::Loop, this) {}

PluginThread::~PluginThread()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void PluginThread::Run(const std::function<void()> &call)
{
    if (std::this_thread::get_id() == thread_.get_id()) {
        call();
        return;
    }
    std::mutex done_mutex;
    std::condition_variable done_cv;
    bool done = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back([&] {
            call();
            std::lock_guard<std::mutex> done_lock(done_mutex);
            done = true;
            done_cv.notify_one();
        });
    }
    wake_.notify_one();
    std::unique_lock<std::mutex> done_lock(done_mutex);
    done_cv.wait(done_lock, [&] { return done; });
}

void PluginThread::Loop()
{
    for (;;) {
        std::function<void()> call;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (queue_.empty())
                return;
            call = std::move(queue_.front());
            queue_.pop_front();
        }
        call();
    }
}

void SerialisePlugin(const void *vtable)
{
    Registry &registry = TheRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::unique_ptr<PluginThread> &thread = registry.threads[vtable];
    if (!thread)
        thread = std::make_unique<PluginThread>();
}

void CallPlugin(const void *vtable, const std::function<void()> &call)
{
    PluginThread *thread = nullptr;
    {
        Registry &registry = TheRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        const auto found = registry.threads.find(vtable);
        if (found != registry.threads.end())
            thread = found->second.get();
    }
    if (thread)
        thread->Run(call);
    else
        call();
}

} // namespace core
