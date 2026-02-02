// voice_filter.cpp
#include "voice_filter.hpp"
#include <cmath>
#include <algorithm>

// Very small biquad notch implementation (pole-zero notch)
struct Biquad { float b0,b1,b2,a1,a2; float s1=0.0f, s2=0.0f; };

// Pole-zero notch: zeros at exp(±jω0), poles at r*exp(±jω0)
// Q -> bandwidth: BW = fc / Q, r = exp(-pi * BW / fs)
static Biquad makeNotch(float fc, float fs, float Q) {
    const float PI = std::acos(-1.0f);
    float omega = 2.0f * PI * fc / fs;
    float cs = std::cos(omega);

    // bandwidth in Hz and pole radius
    float BW = fc / Q;
    float r = std::exp(-PI * BW / fs);

    // numerator (zeros on unit circle)
    float b0 = 1.0f;
    float b1 = -2.0f * cs;
    float b2 = 1.0f;

    // denominator (poles at r*e^{±jω0})
    float a1 = -2.0f * r * cs;
    float a2 = r * r;

    // a0 = 1 (normalized)
    return { b0, b1, b2, a1, a2, 0.0f, 0.0f };
}

// apply biquad (transposed DF-II)
static inline float applyBiquad(float x, Biquad &f) {
    float y = f.b0 * x + f.s1;
    float ns1 = f.b1 * x - f.a1 * y + f.s2;
    float ns2 = f.b2 * x - f.a2 * y;
    f.s1 = ns1; f.s2 = ns2;
    return y;
}

VoiceGenderFilter::VoiceGenderFilter() = default;

void VoiceGenderFilter::processChunk(std::vector<float>& audio) {
    if (!male_notch || audio.empty()) return;

    const float fs = 48000.0f;

    // frequencies and per-notch Q multipliers (effectiveQ = notch_Q * factor)
    const std::pair<float,float> notches[] = {
        {135.0f, 1.0f/7.0f},   // n1: notch_Q/7
        {275.0f, 2.0f},        // n2: notch_Q*2
        {561.0f, 2.0f},        // n3: notch_Q*2
        {396.0f, 4.0f},        // n4: notch_Q*4
        {530.0f, 2.0f},        // n5: notch_Q*2  (was commented out previously)
        {50.0f,  1.0f/5.0f},   // n6: notch_Q/5
        {105.0f, 1.0f/7.0f},   // n7: notch_Q/7
        {4088.0f,1.0f/5.0f},   // n8: notch_Q/5
        {68.0f,  1.0f/5.0f},   // n9: notch_Q/5
        {131.0f, 1.0f/5.0f}    // n10: notch_Q/5
    };

    // build biquads for each notch (states initialized zero)
    Biquad filters[sizeof(notches)/sizeof(notches[0])];
    for (size_t i = 0; i < std::size(filters); ++i) {
        float fc = notches[i].first;
        float factor = notches[i].second;
        float effQ = notch_Q * factor;
        filters[i] = makeNotch(fc, fs, effQ);
    }

    for (auto &s : audio) {
        float x = s;
        // apply all notches in sequence (preserves original ordering/behavior)
        for (size_t i = 0; i < std::size(filters); ++i) x = applyBiquad(x, filters[i]);
        // safe clamp to audio range
        s = std::clamp(x, -1.0f, 1.0f);
    }
}
