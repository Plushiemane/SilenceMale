// voice_filter.hpp
#pragma once
#include <vector>

class VoiceGenderFilter {
public:
    VoiceGenderFilter();
    // processChunk applies the simple 3‑notch filter when enabled
    void processChunk(std::vector<float>& audio);

    // API used by tests
    void setFemaleBoost(bool enable) { (void)enable; } // kept so existing calls compile
    void enableMaleNotch(bool enable) { male_notch = enable; }
    void setMaleNotchQ(float q) { notch_Q = q; }

private:
    bool male_notch = true;
    float notch_Q = 30.0f; // default narrow notch
};