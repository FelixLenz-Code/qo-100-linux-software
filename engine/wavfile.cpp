#include "wavfile.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace qo100::wavfile {

namespace {
void putU32(std::FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }
void putU16(std::FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }

int16_t toI16(float x) {
    if (x > 1.0f) x = 1.0f;
    else if (x < -1.0f) x = -1.0f;
    return static_cast<int16_t>(x * 32767.0f);
}

// Checked read: returns false (and lets the caller bail) on short reads.
bool rd(void* p, size_t size, size_t count, std::FILE* f) {
    return std::fread(p, size, count, f) == count;
}
} // namespace

bool writeMono(const std::string& path, const std::vector<float>& samples, int sampleRate) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    const uint16_t channels = 1, bits = 16;
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * channels * bits / 8;

    std::fwrite("RIFF", 1, 4, f);
    putU32(f, 36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    putU32(f, 16);                 // fmt chunk size
    putU16(f, 1);                  // PCM
    putU16(f, channels);
    putU32(f, static_cast<uint32_t>(sampleRate));
    putU32(f, byteRate);
    putU16(f, static_cast<uint16_t>(channels * bits / 8)); // block align
    putU16(f, bits);
    std::fwrite("data", 1, 4, f);
    putU32(f, dataBytes);
    for (float s : samples) {
        const int16_t v = toI16(s);
        std::fwrite(&v, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

bool readMono(const std::string& path, std::vector<float>& samples, int& sampleRate) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char riff[4], wave[4];
    uint32_t sz;
    if (!rd(riff, 1, 4, f) || !rd(&sz, 4, 1, f) || !rd(wave, 1, 4, f) ||
        std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) {
        std::fclose(f);
        return false;
    }
    uint16_t channels = 1, bits = 16;
    sampleRate = 0;
    // Walk chunks until we hit "data".
    for (;;) {
        char id[4];
        uint32_t size;
        if (!rd(id, 1, 4, f) || !rd(&size, 4, 1, f)) {
            std::fclose(f);
            return false;
        }
        if (std::memcmp(id, "fmt ", 4) == 0) {
            uint16_t fmt, blockAlign;
            uint32_t sr, byteRate;
            if (!rd(&fmt, 2, 1, f) || !rd(&channels, 2, 1, f) || !rd(&sr, 4, 1, f) ||
                !rd(&byteRate, 4, 1, f) || !rd(&blockAlign, 2, 1, f) || !rd(&bits, 2, 1, f)) {
                std::fclose(f);
                return false;
            }
            sampleRate = static_cast<int>(sr);
            std::fseek(f, size - 16, SEEK_CUR); // skip any extra fmt bytes
        } else if (std::memcmp(id, "data", 4) == 0) {
            const uint32_t count = size / (bits / 8);
            samples.resize(count);
            for (uint32_t i = 0; i < count; ++i) {
                int16_t v;
                if (!rd(&v, 2, 1, f)) {
                    std::fclose(f);
                    return false;
                }
                samples[i] = v / 32768.0f;
            }
            std::fclose(f);
            return channels == 1;
        } else {
            std::fseek(f, size, SEEK_CUR); // skip unknown chunk
        }
    }
}

} // namespace qo100::wavfile
