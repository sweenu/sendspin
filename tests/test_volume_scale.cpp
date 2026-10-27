#include "volume_scale.h"
#include <cassert>
#include <cmath>

int main() {
    // --- dB controls: gain = 30 * log10(v / 100) dB below the control's max ---
    // e.g. a DAC+ "Digital" control: -103.5 dB .. 0 dB
    const long MIN = -10350, MAX = 0;
    assert(sendspin_to_centidb(100, MIN, MAX) == 0);
    assert(sendspin_to_centidb(0, MIN, MAX) == MIN);
    // volume 50 is amplitude 0.354, -9.03 dB (not half the dB range)
    assert(sendspin_to_centidb(50, MIN, MAX) == -903);
    assert(sendspin_to_centidb(10, MIN, MAX) == -3000);
    assert(sendspin_to_centidb(1, MIN, MAX) == -6000);
    // the curve is relative to the control's max, so boost stays reachable
    assert(sendspin_to_centidb(100, -6000, 600) == 600);
    assert(sendspin_to_centidb(50, -6000, 600) == 600 - 903);
    // gains below the control's range clamp to its min
    assert(sendspin_to_centidb(10, -2000, 0) == -2000);
    // an out-of-range input (e.g. a stray 0-255 value) is clamped to 100
    assert(sendspin_to_centidb(171, MIN, MAX) == MAX);

    assert(centidb_to_sendspin(0, MIN, MAX) == 100);
    assert(centidb_to_sendspin(MIN, MIN, MAX) == 0);
    assert(centidb_to_sendspin(-903, MIN, MAX) == 50);
    assert(centidb_to_sendspin(-3000, MIN, MAX) == 10);
    assert(centidb_to_sendspin(500, MIN, MAX) == 100);
    // ALSA's mute gain (SND_CTL_TLV_DB_GAIN_MUTE) reads as 0
    assert(centidb_to_sendspin(-9999999, MIN, MAX) == 0);
    // every volume survives a round trip through a 0.5 dB-step control
    for (int v = 0; v <= 100; ++v) {
        long cdb = sendspin_to_centidb(static_cast<uint8_t>(v), MIN, MAX);
        long stepped = std::lround(cdb / 50.0) * 50;
        int back = centidb_to_sendspin(stepped, MIN, MAX);
        assert(std::abs(back - v) <= 2);
    }

    // --- controls without dB: raw steps as linear amplitude ---
    assert(sendspin_to_raw(0, 0, 1000) == 0);
    assert(sendspin_to_raw(100, 0, 1000) == 1000);
    assert(sendspin_to_raw(50, 0, 1000) == 354);
    assert(sendspin_to_raw(0, -10239, 400) == -10239);
    assert(sendspin_to_raw(100, -10239, 400) == 400);
    assert(sendspin_to_raw(171, 0, 1000) == 1000);
    assert(raw_to_sendspin(0, 0, 1000) == 0);
    assert(raw_to_sendspin(1000, 0, 1000) == 100);
    assert(raw_to_sendspin(354, 0, 1000) == 50);
    assert(raw_to_sendspin(1500, 0, 1000) == 100);
    assert(raw_to_sendspin(-5, 0, 1000) == 0);
    for (int v = 0; v <= 100; ++v)
        assert(raw_to_sendspin(sendspin_to_raw(static_cast<uint8_t>(v), 0, 1000), 0, 1000) == v);

    // degenerate ranges
    assert(sendspin_to_centidb(50, 0, 0) == 0);
    assert(centidb_to_sendspin(0, 0, 0) == 0);
    assert(sendspin_to_raw(50, 5, 5) == 5);
    assert(raw_to_sendspin(5, 5, 5) == 0);
    return 0;
}
