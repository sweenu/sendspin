#pragma once
#include <cstdint>

// Sendspin volume (0-100) is perceived loudness, not amplitude. The player role
// spec maps it to a linear amplitude of (volume / 100)^1.5, which is a gain of
// 30 * log10(volume / 100) dB. Volume 100 is the control at its maximum and
// volume 0 is the control at its minimum.

// For controls with a dB scale. dB values are in ALSA's 1/100 dB units.
long sendspin_to_centidb(uint8_t v, long min_cdb, long max_cdb);
uint8_t centidb_to_sendspin(long cdb, long min_cdb, long max_cdb);

// For controls that report no dB scale: raw steps are taken as linear amplitude.
long sendspin_to_raw(uint8_t v, long min, long max);
uint8_t raw_to_sendspin(long raw, long min, long max);
