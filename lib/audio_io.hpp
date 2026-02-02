#pragma once
#include <string>
#include <vector>

std::vector<float> loadAudioFile(const std::string& path); // returns mono float32 @44100 or fallback tone