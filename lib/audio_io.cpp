#include "audio_io.hpp"
#include "wav_io.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

std::vector<float> loadAudioFile(const std::string& path) {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) {
        std::cerr << "loadAudioFile: input not found: " << path << "\n";
        return {}; // caller will fall back if desired
    }

    // unique tmp name
    std::size_t h = std::hash<std::string>{}(fs::absolute(path).string());
    std::string tmp = "/tmp/silencemale_" + std::to_string(h) + ".wav";

    // Use explicit PCM float codec (pcm_f32le) which WAV muxer accepts
    std::string cmd = "ffmpeg -y -nostdin -v error -i \"" + path +
                      "\" -f wav -ar 48000 -ac 1 -c:a pcm_f32le \"" + tmp + "\"";
    std::cerr << "Running: " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    std::cerr << "ffmpeg exit code: " << rc << " tmp=" << tmp << "\n";

    if (rc != 0 || !fs::exists(tmp)) {
        std::cerr << "ffmpeg failed or tmp WAV missing -> returning empty\n";
        if (fs::exists(tmp)) std::remove(tmp.c_str());
        return {};
    }

    auto samples = readWavAsFloats(tmp);
    std::remove(tmp.c_str());
    if (samples.empty()) {
        std::cerr << "readWavAsFloats produced no samples\n";
    }
    return samples;
}