// SPDX-License-Identifier: MIT
// Keeps a DPF editor alive in hosts that never call effEditIdle (notably OBS
// Studio's built-in VST 2.x filter on Linux, which has no plugin-UI idle
// callback), without ever fighting a host that does idle us.
//
// Xlib is not thread-safe, so the worker thread and the host must never pump the
// editor at the same time.  The rules are therefore conservative:
//
//   * a host that idles the editor is never helped: as soon as we see more than
//     a couple of host idles the worker retires for good;
//   * the worker only starts pumping when the host is *known* not to idle
//     (the host process is OBS Studio, or LSTRIP_SELF_IDLE=1), or after a long
//     grace period (kGraceSec) during which the host idled at most once -- DPF
//     itself idles once when the editor opens, which does not count.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>

class IdleDriver {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr double kGraceSec = 3.0;

    explicit IdleDriver(std::function<void()> pump) : pump_(std::move(pump)) {}
    ~IdleDriver() { stop(); }

    static bool hostKnownIdleless() {
        if (std::getenv("LSTRIP_NO_SELF_IDLE")) return false;
        if (std::getenv("LSTRIP_SELF_IDLE")) return true;
        char comm[64] = {};
        if (std::FILE* f = std::fopen("/proc/self/comm", "r")) { if (!std::fgets(comm, sizeof comm, f)) comm[0] = 0; std::fclose(f); }
        return std::strncmp(comm, "obs", 3) == 0;   // "obs", "obs-studio", ...
    }

    void start() {
        if (running_.exchange(true)) return;
        const bool immediate = hostKnownIdleless();
        thread_ = std::thread([this, immediate] {
            const auto t0 = Clock::now();
            while (running_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                if (!running_.load()) break;
                if (hostIdles_.load() > 2) break;                         // host is driving us: retire
                const double age = std::chrono::duration<double>(Clock::now() - t0).count();
                if (!immediate && age < kGraceSec) continue;              // still waiting to see what the host does
                if (!immediate && hostIdles_.load() > 1) continue;        // looks like it idles after all
                pump_();
                selfDriven_.store(true);
            }
        });
    }
    void stop() {
        if (!running_.exchange(false)) return;
        if (thread_.joinable()) thread_.join();
    }

    // Call from UI::uiIdle() (the host-driven path).
    void noteHostIdle() { hostIdles_.fetch_add(1); }
    bool selfDriven() const { return selfDriven_.load(); }

private:
    std::function<void()> pump_;
    std::thread thread_;
    std::atomic<bool> running_{false}, selfDriven_{false};
    std::atomic<int> hostIdles_{0};
};
