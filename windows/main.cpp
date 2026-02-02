#include <portaudio.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cmath>

struct Biquad {
    float b0,b1,b2,a1,a2;
    float s1=0.0f, s2=0.0f;
    inline float process(float x) {
        float y = b0 * x + s1;
        float ns1 = b1 * x - a1 * y + s2;
        float ns2 = b2 * x - a2 * y;
        s1 = ns1; s2 = ns2;
        return y;
    }
};

// Pole-zero notch: zeros at exp(±jω0), poles at r*exp(±jω0)
// Q -> bandwidth: BW = fc / Q, r = exp(-pi * BW / fs)
static Biquad makeNotch(float fc, float fs, float Q) {
    const float PI = std::acos(-1.0f);
    float omega = 2.0f * PI * fc / fs;
    float cs = std::cos(omega);
    float BW = fc / Q;
    float r = std::exp(-PI * BW / fs);
    float b0 = 1.0f;
    float b1 = -2.0f * cs;
    float b2 = 1.0f;
    float a1 = -2.0f * r * cs;
    float a2 = r * r;
    return { b0, b1, b2, a1, a2, 0.0f, 0.0f };
}

struct UserData {
    Biquad n1, n2, n3;
    std::atomic<bool> running{true};
    float gain = 1.0f; // post gain
};

static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                         void* userData)
{
    auto* d = static_cast<UserData*>(userData);
    const float* in = static_cast<const float*>(inputBuffer);
    float* out = static_cast<float*>(outputBuffer);
    if (!in) {
        // silence if no input
        std::fill(out, out + framesPerBuffer, 0.0f);
        return paContinue;
    }
    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float x = in[i];
        x = d->n1.process(x);
        x = d->n2.process(x);
        x = d->n3.process(x);
        x *= d->gain;
        // clamp
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        out[i] = x;
    }
    return paContinue;
}

int main() {
    PaError err = Pa_Initialize();
    if (err != paNoError) { std::cerr << "PortAudio init error\n"; return 1; }

    int devCount = Pa_GetDeviceCount();
    if (devCount < 0) { std::cerr << "Pa_GetDeviceCount error\n"; Pa_Terminate(); return 1; }

    int inputDev = -1;
    int outputDev = -1;
    std::cerr << "Devices:\n";
    for (int i = 0; i < devCount; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        std::string name = info->name ? info->name : "";
        std::cerr << i << ": " << name
                  << " (in:" << info->maxInputChannels
                  << " out:" << info->maxOutputChannels << ")\n";
        if (inputDev < 0 && info->maxInputChannels > 0) inputDev = i;
        // prefer virtual cable-like outputs
        std::string lname = name;
        std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
        if (outputDev < 0 && info->maxOutputChannels > 0 &&
            (lname.find("cable") != std::string::npos || lname.find("vb-audio") != std::string::npos || lname.find("virtual") != std::string::npos)) {
            outputDev = i;
        }
    }
    if (inputDev < 0) { std::cerr << "No input device found\n"; Pa_Terminate(); return 1; }
    if (outputDev < 0) {
        outputDev = Pa_GetDefaultOutputDevice();
        std::cerr << "No virtual cable output found; using default output (" << outputDev << ")\n";
        std::cerr << "Install VB-Cable and run again if you want a virtual microphone endpoint.\n";
    }

    const PaDeviceInfo* inInfo = Pa_GetDeviceInfo(inputDev);
    const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(outputDev);
    double fs = std::min(inInfo->defaultSampleRate, outInfo->defaultSampleRate);
    unsigned long framesPerBuffer = 256;

    PaStreamParameters inParams, outParams;
    inParams.device = inputDev;
    inParams.channelCount = 1;
    inParams.sampleFormat = paFloat32;
    inParams.suggestedLatency = inInfo->defaultLowInputLatency;
    inParams.hostApiSpecificStreamInfo = nullptr;

    outParams.device = outputDev;
    outParams.channelCount = 1;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = outInfo->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    UserData data;
    // notches centered at male harmonics (keep Qs as user set previously)
    const float Q = 60.0f; // leave Q adjustable as you like
    data.n1 = makeNotch(131.0f, float(fs), Q);
    data.n2 = makeNotch(275.0f, float(fs), Q);
    data.n3 = makeNotch(561.0f, float(fs), Q);
    data.gain = 1.0f; // you can set to 10.0f for +20dB if desired

    PaStream* stream = nullptr;
    err = Pa_OpenStream(&stream, &inParams, &outParams, fs, framesPerBuffer, paNoFlag, audioCallback, &data);
    if (err != paNoError) { std::cerr << "OpenStream error: " << Pa_GetErrorText(err) << "\n"; Pa_Terminate(); return 1; }

    err = Pa_StartStream(stream);
    if (err != paNoError) { std::cerr << "StartStream error\n"; Pa_CloseStream(stream); Pa_Terminate(); return 1; }

    std::cerr << "Running. Capturing from device " << inputDev << " -> output device " << outputDev << "\n";
    std::cerr << "Press Enter to stop.\n";
    std::cin.get();

    data.running = false;
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}