#include "volume_scale.h"
#include <cmath>

// The Sendspin wire protocol carries volume as 0-100 (Music Assistant's
// aiosendspin server rejects a client/state whose volume is outside 0-100).
// Note: sendspin-cpp's PlayerRole API documents volume as 0-255, but the value
// is transmitted verbatim, so the daemon works in 0-100 end to end.
static constexpr long SENDSPIN_VOL_MAX = 100;
// amplitude = (volume / 100)^1.5
static constexpr double CURVE_EXPONENT = 1.5;

static uint8_t clamp_volume(long v) {
    if (v < 0) return 0;
    if (v > SENDSPIN_VOL_MAX) return static_cast<uint8_t>(SENDSPIN_VOL_MAX);
    return static_cast<uint8_t>(v);
}

long sendspin_to_centidb(uint8_t v, long min_cdb, long max_cdb) {
    if (max_cdb <= min_cdb) return min_cdb;
    if (v == 0) return min_cdb;
    if (v >= SENDSPIN_VOL_MAX) return max_cdb;
    // 20 * log10(amplitude) dB, in 1/100 dB
    double gain = 2000.0 * CURVE_EXPONENT * std::log10(v / static_cast<double>(SENDSPIN_VOL_MAX));
    long cdb = max_cdb + std::lround(gain);
    return cdb < min_cdb ? min_cdb : cdb;
}

uint8_t centidb_to_sendspin(long cdb, long min_cdb, long max_cdb) {
    if (max_cdb <= min_cdb) return 0;
    if (cdb <= min_cdb) return 0;
    if (cdb >= max_cdb) return static_cast<uint8_t>(SENDSPIN_VOL_MAX);
    double amplitude = std::pow(10.0, (cdb - max_cdb) / 2000.0);
    return clamp_volume(std::lround(SENDSPIN_VOL_MAX * std::pow(amplitude, 1.0 / CURVE_EXPONENT)));
}

long sendspin_to_raw(uint8_t v, long min, long max) {
    if (max <= min) return min;
    if (v > SENDSPIN_VOL_MAX) v = static_cast<uint8_t>(SENDSPIN_VOL_MAX);
    double amplitude = std::pow(v / static_cast<double>(SENDSPIN_VOL_MAX), CURVE_EXPONENT);
    return min + std::lround(amplitude * (max - min));
}

uint8_t raw_to_sendspin(long raw, long min, long max) {
    if (max <= min) return 0;
    if (raw <= min) return 0;
    if (raw >= max) return static_cast<uint8_t>(SENDSPIN_VOL_MAX);
    double amplitude = (raw - min) / static_cast<double>(max - min);
    return clamp_volume(std::lround(SENDSPIN_VOL_MAX * std::pow(amplitude, 1.0 / CURVE_EXPONENT)));
}
