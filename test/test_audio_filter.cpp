// test_audio_filter.cpp
#include "../lib/voice_filter.hpp"
#include "../lib/audio_io.hpp"
#include "../lib/wav_io.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>

constexpr double PI = 3.14159265358979323846;

// small helpers (RMS etc) — unchanged and minimal
static double computeRMS(const std::vector<float>& audio) {
    if (audio.empty()) return 0.0;
    double s = 0;
    for (float v : audio) s += double(v) * double(v);
    return std::sqrt(s / audio.size());
}

// Goertzel power for single frequency
static double goertzelPower(const std::vector<float>& x, double fs, double freq) {
    if (x.empty()) return 0.0;
    double kf = freq * x.size() / fs;
    double omega = 2.0 * M_PI * kf / x.size();
    double coeff = 2.0 * std::cos(omega);
    double s0=0, s1=0, s2=0;
    for (size_t i=0;i<x.size();++i) {
        s0 = x[i] + coeff*s1 - s2;
        s2 = s1; s1 = s0;
    }
    double real = s1 - s2 * std::cos(omega);
    double imag = s2 * std::sin(omega);
    return real*real + imag*imag;
}

// helper to print before/after powers (dB)
static void printNotchReport(const std::vector<float>& before, const std::vector<float>& after, double fs) {
    std::vector<double> freqs = {131.0, 275.0, 561.0}; // target male harmonics
    std::cout << "Freq(Hz)   before(dB)   after(dB)   delta(dB)\n";
    for (double f : freqs) {
        double p_before = goertzelPower(before, fs, f) + 1e-18;
        double p_after  = goertzelPower(after,  fs, f) + 1e-18;
        double db_before = 10.0 * std::log10(p_before);
        double db_after  = 10.0 * std::log10(p_after);
        std::cout << std::setw(7) << int(f)
                  << "    " << std::setw(8) << db_before
                  << "    " << std::setw(8) << db_after
                  << "    " << std::setw(7) << (db_after - db_before) << "\n";
    }
}

int main() {
    std::cout << "=== Audio Filter Test ===\n";
    struct TestFile { const char* in; const char* out; };
    TestFile files[] = {
        { "test/male.m4a", "test/male_processed.wav" },
        { "test/female.m4a", "test/female_processed.wav" }
    };

    for (auto &t : files) {
        std::cout << "\nProcessing: " << t.in << std::endl;
        auto audio = loadAudioFile(t.in);
        if (audio.empty()) { std::cerr << "no audio\n"; continue; }

        VoiceGenderFilter filter;            // minimal stub / your notch implementation lives here
        filter.enableMaleNotch(true);        // API preserved
        filter.setMaleNotchQ(2.8f);

        double rms_before = computeRMS(audio);
        std::cout << "Samples: " << audio.size() << "  RMS before: " << rms_before << "\n";

        double fs = 48000.0; // match your IO
        auto before = audio;
        filter.processChunk(audio);
        auto after = audio;
        printNotchReport(before, after, fs);

        double rms_after = computeRMS(audio);
        std::cout << "RMS after: " << rms_after << "\n";

        writeWAVFile(t.out, audio);
        std::cout << "Wrote: " << t.out << "\n";
    }
    return 0;
}