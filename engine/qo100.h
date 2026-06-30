#pragma once

// QO-100 (Es'hail-2) narrowband transponder frequency plan, in Hz.
// The transponder is non-inverting: downlink = uplink + LO offset.
namespace qo100::plan {

constexpr double kUplinkLow = 2400.000e6;   // 13 cm, TX
constexpr double kUplinkHigh = 2400.500e6;
constexpr double kDownlinkLow = 10489.500e6; // 3 cm, RX
constexpr double kDownlinkHigh = 10490.000e6;

constexpr double kLoOffset = 8089.500e6; // downlink = uplink + kLoOffset

// Beacons (downlink): two CW at the band edges, a BPSK reference in the middle.
constexpr double kBeaconLowerCW = 10489.500e6;
constexpr double kBeaconMiddlePSK = 10489.750e6;
constexpr double kBeaconUpperCW = 10490.000e6;

inline double uplinkForDownlink(double downlinkHz) { return downlinkHz - kLoOffset; }
inline double downlinkForUplink(double uplinkHz) { return uplinkHz + kLoOffset; }

} // namespace qo100::plan
