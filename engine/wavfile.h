#pragma once
#include <string>
#include <vector>

// Minimal mono 16-bit PCM WAV I/O — enough to render decoded audio to a file
// you can listen to, and to read it back for verification.
namespace qo100::wavfile {

bool writeMono(const std::string& path, const std::vector<float>& samples, int sampleRate);
bool readMono(const std::string& path, std::vector<float>& samples, int& sampleRate);

} // namespace qo100::wavfile
