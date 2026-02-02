#include "wav_io.hpp"
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>

std::vector<float> readWavAsFloats(const std::string& wavFile) {
    std::ifstream in(wavFile, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "readWavAsFloats: cannot open " << wavFile << "\n";
        return {};
    }

    // RIFF header (12 bytes)
    char riff[4]; uint32_t fileSize; char wave[4];
    in.read(riff,4); in.read(reinterpret_cast<char*>(&fileSize),4); in.read(wave,4);
    if (in.fail() || std::strncmp(riff,"RIFF",4)!=0 || std::strncmp(wave,"WAVE",4)!=0) {
        std::cerr << "Not a RIFF/WAVE file: " << wavFile << "\n";
        return {};
    }

    // Walk chunks until "fmt " and "data" found
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0, dataSize = 0;
    std::streampos dataPos = -1;

    while (!in.eof()) {
        char chunkId[4]; uint32_t chunkSize = 0;
        in.read(chunkId,4);
        if (in.eof() || in.fail()) break;
        in.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (in.fail()) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            // read fmt chunk (at least 16 bytes)
            uint16_t fmt1 = 0;
            in.read(reinterpret_cast<char*>(&fmt1), sizeof(fmt1)); // audioFormat
            in.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
            in.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
            uint32_t byteRate = 0; in.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
            uint16_t blockAlign = 0; in.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
            in.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));
            audioFormat = fmt1;
            // skip any extra bytes in fmt chunk
            if (chunkSize > 16) in.seekg(int(chunkSize - 16), std::ios::cur);
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            dataPos = in.tellg();
            in.seekg(chunkSize, std::ios::cur);
        } else {
            // skip unknown chunk
            in.seekg(chunkSize, std::ios::cur);
        }
        // align to word if chunkSize odd
        if (chunkSize & 1) in.seekg(1, std::ios::cur);
    }

    if (dataPos == -1) {
        std::cerr << "No data chunk found in WAV: " << wavFile << "\n";
        return {};
    }
    if (audioFormat == 0) {
        std::cerr << "fmt chunk missing or unreadable in WAV: " << wavFile << "\n";
        return {};
    }

    // read samples
    in.clear();
    in.seekg(dataPos);
    size_t sampleCount = static_cast<size_t>(dataSize) / (bitsPerSample / 8);
    std::vector<float> samples;
    samples.reserve(sampleCount);

    if (audioFormat == 3 && bitsPerSample == 32) {
        // IEEE float
        for (size_t i = 0; i < sampleCount; ++i) {
            float v=0.0f;
            if (!in.read(reinterpret_cast<char*>(&v), sizeof(v))) break;
            samples.push_back(v);
        }
    } else if (audioFormat == 1 && bitsPerSample == 16) {
        for (size_t i = 0; i < sampleCount; ++i) {
            int16_t pcm = 0;
            if (!in.read(reinterpret_cast<char*>(&pcm), sizeof(pcm))) break;
            samples.push_back(static_cast<float>(pcm) / 32768.0f);
        }
    } else {
        std::cerr << "Unsupported WAV format: audioFormat=" << audioFormat
                  << " bits=" << bitsPerSample << "\n";
    }

    return samples;
}

void writeWAVFile(const std::string& filename, const std::vector<float>& samples, int sampleRate) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) { std::cerr << "cannot open " << filename << "\n"; return; }

    // write 32-bit float WAV ("fmt " chunk 16 bytes)
    auto write4 = [&](const char *s){ file.write(s,4); };
    write4("RIFF");
    uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(float));
    uint32_t fileSize = 36 + dataBytes;
    file.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
    write4("WAVE");
    write4("fmt ");
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), sizeof(fmtSize));
    uint16_t audioFormat = 3; // float
    uint16_t numChannels = 1;
    file.write(reinterpret_cast<const char*>(&audioFormat), sizeof(audioFormat));
    file.write(reinterpret_cast<const char*>(&numChannels), sizeof(numChannels));
    uint32_t sr = static_cast<uint32_t>(sampleRate);
    file.write(reinterpret_cast<const char*>(&sr), sizeof(sr));
    uint32_t byteRate = sr * numChannels * sizeof(float);
    file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
    uint16_t blockAlign = numChannels * sizeof(float);
    file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
    uint16_t bitsPerSample = 32;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
    write4("data");
    file.write(reinterpret_cast<const char*>(&dataBytes), sizeof(dataBytes));
    for (float v : samples) {
        float clamped = std::max(-1.0f, std::min(1.0f, v));
        file.write(reinterpret_cast<const char*>(&clamped), sizeof(clamped));
    }
    file.close();
}