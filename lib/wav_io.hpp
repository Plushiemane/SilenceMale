#pragma once
#include <string>
#include <vector>

std::vector<float> readWavAsFloats(const std::string& wavFile);
void writeWAVFile(const std::string& filename, const std::vector<float>& samples, int sampleRate = 44100);