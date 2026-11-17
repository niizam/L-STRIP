// SPDX-License-Identifier: MIT
// Process-local side channel from DSP to UI.
//
// DPF's VST2 wrapper only delivers output parameters (meters) to the editor from
// the host's effEditIdle call.  OBS Studio never makes that call on Linux, so the
// editor would never see a meter value.  Instead every plugin instance publishes
// its parameter values in a SharedState that the editor reads directly.
#pragma once
#include "Params.hpp"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace lstrip {

struct SharedState {
    std::atomic<float> value[kParamCount];
    SharedState() { for (uint32_t i = 0; i < kParamCount; ++i) value[i].store(kParams[i].def, std::memory_order_relaxed); }
    void set(uint32_t i, float v) { value[i].store(v, std::memory_order_relaxed); }
    float get(uint32_t i) const { return value[i].load(std::memory_order_relaxed); }
};

// DPF hands the editor a pointer to the DSP object (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS)
// for in-process formats.  The plugin registers its SharedState under that pointer;
// the editor looks it up.  When the editor lives in another process or another shared
// object (LV2 UI bundle) the lookup finds nothing and the editor falls back to DPF's
// regular parameterChanged() updates.  No RTTI is involved, so this links everywhere.
class SharedRegistry {
public:
    static SharedRegistry& instance() { static SharedRegistry r; return r; }
    void add(const void* key, const std::shared_ptr<SharedState>& s) {
        std::lock_guard<std::mutex> lk(m_);
        map_[key] = s;
    }
    void remove(const void* key) {
        std::lock_guard<std::mutex> lk(m_);
        map_.erase(key);
    }
    std::shared_ptr<SharedState> find(const void* key) {
        if (!key) return nullptr;
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(key);
        return it == map_.end() ? nullptr : it->second.lock();
    }

private:
    std::mutex m_;
    std::map<const void*, std::weak_ptr<SharedState>> map_;
};

} // namespace lstrip
