#pragma once
#include <complex>
#include <string>
#include <vector>

// Interleaved float32 complex IQ files (.cf32 / "fc32"), little-endian I,Q,I,Q...
// This is the de-facto interchange format for SDR recordings (GNU Radio,
// SDR++, gqrx all read/write it), so real QO-100 captures drop straight in.
namespace qo100::iqfile {

using cf32 = std::complex<float>;

bool write(const std::string& path, const std::vector<cf32>& iq);
bool read(const std::string& path, std::vector<cf32>& iq);

} // namespace qo100::iqfile
