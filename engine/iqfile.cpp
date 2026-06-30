#include "iqfile.h"
#include <cstdio>

namespace qo100::iqfile {

bool write(const std::string& path, const std::vector<cf32>& iq) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    // std::complex<float> is guaranteed to be laid out as two contiguous floats,
    // so the whole vector is already interleaved I,Q,I,Q.
    const size_t n = std::fwrite(iq.data(), sizeof(cf32), iq.size(), f);
    std::fclose(f);
    return n == iq.size();
}

bool read(const std::string& path, std::vector<cf32>& iq) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long bytes = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (bytes < 0 || bytes % static_cast<long>(sizeof(cf32)) != 0) {
        std::fclose(f);
        return false;
    }
    iq.resize(static_cast<size_t>(bytes) / sizeof(cf32));
    const size_t n = std::fread(iq.data(), sizeof(cf32), iq.size(), f);
    std::fclose(f);
    return n == iq.size();
}

} // namespace qo100::iqfile
