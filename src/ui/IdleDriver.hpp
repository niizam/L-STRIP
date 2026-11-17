// SPDX-License-Identifier: MIT
// Keeps a DPF editor alive in hosts that never call effEditIdle (notably OBS
// Studio's built-in VST 2.x filter on Linux, which has no plugin-UI idle
// callback).  If the host stops idling us, a worker thread pumps the DGL event
// loop at ~60 Hz instead; as soon as the host idles again the thread stands
// down.  A mutex serialises the two idle paths.
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

class IdleDriver {
public:
    using Clock = std::chrono::steady_clock;

    // `pump` runs one DGL idle iteration + the UI's own tick (called on the worker thread).
    explicit IdleDriver(std::function<void()> pump) : pump_(std::move(pump)) {
        lastHost_.store(Clock::now().time_since_epoch().count());
    }
    ~IdleDriver() { stop(); }

    void start() {
        if (running_.exchange(true)) return;
        thread_ = std::thread([this] {
            while (running_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                if (!running_.load()) break;
                if (hostIsIdling()) continue;
                std::lock_guard<std::mutex> lk(m_);
                if (running_.load() && !hostIsIdling()) pump_();
                selfDriven_.store(true);
            }
        });
    }
    void stop() {
        if (!running_.exchange(false)) return;
        if (thread_.joinable()) thread_.join();
    }

    // Call from UI::uiIdle() (host-driven path).
    void noteHostIdle() { lastHost_.store(Clock::now().time_since_epoch().count()); }
    // Host-driven idle should take the lock too so the paths never overlap.
    std::unique_lock<std::mutex> lockForHost() { return std::unique_lock<std::mutex>(m_); }
    bool selfDriven() const { return selfDriven_.load(); }

private:
    bool hostIsIdling() const {
        const auto last = Clock::time_point(Clock::duration(lastHost_.load()));
        return Clock::now() - last < std::chrono::milliseconds(250);
    }
    std::function<void()> pump_;
    std::thread thread_;
    std::mutex m_;
    std::atomic<bool> running_{false}, selfDriven_{false};
    std::atomic<long long> lastHost_{0};
};
