// SPDX-License-Identifier: MIT
// Placeholder UI (replaced by the full interface later in this milestone).
#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class LStripUI : public UI {
public:
    LStripUI() : UI(940, 640) {}
protected:
    void parameterChanged(uint32_t, float) override {}
    void onNanoDisplay() override {
        beginPath(); rect(0, 0, getWidth(), getHeight()); fillColor(10, 14, 22); fill();
    }
};

UI* createUI() { return new LStripUI(); }

END_NAMESPACE_DISTRHO
