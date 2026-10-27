// src/volume_control.cpp
#include "volume_control.h"
#include "volume_scale.h"
#include <cstdio>
#include <cstdlib>
#include <utility>

VolumeControl::VolumeControl(std::string device, std::string control)
    : device_(std::move(device)), control_(std::move(control)) {}

VolumeControl::~VolumeControl() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (handle_) snd_mixer_close(handle_);
}

bool VolumeControl::open() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (control_.empty()) {
        std::fprintf(stderr, "volume: no mixer control name; volume disabled\n");
        return false;
    }
    if (snd_mixer_open(&handle_, 0) < 0) return false;
    if (snd_mixer_attach(handle_, device_.c_str()) < 0) return false;
    if (snd_mixer_selem_register(handle_, nullptr, nullptr) < 0) return false;
    if (snd_mixer_load(handle_) < 0) return false;

    snd_mixer_selem_id_t* sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, control_.c_str());
    elem_ = snd_mixer_find_selem(handle_, sid);
    if (!elem_) {
        std::fprintf(stderr, "volume: control '%s' not found on %s\n",
                     control_.c_str(), device_.c_str());
        return false;
    }
    snd_mixer_selem_get_playback_volume_range(elem_, &min_, &max_);
    // Prefer the control's dB scale so the Sendspin loudness curve lands on
    // real gain; without one, raw steps are taken as linear amplitude.
    has_db_ = snd_mixer_selem_get_playback_dB_range(elem_, &min_cdb_, &max_cdb_) == 0 &&
              max_cdb_ > min_cdb_;
    std::fprintf(stderr, "volume: control '%s' raw %ld..%ld, %s\n", control_.c_str(),
                 min_, max_, has_db_ ? "dB scale" : "no dB scale");
    return true;
}

// Raw step whose gain is nearest the curve's target for v.
long VolumeControl::raw_for_volume(uint8_t v) {
    if (!has_db_) return sendspin_to_raw(v, min_, max_);
    if (v == 0) return min_;
    long target = sendspin_to_centidb(v, min_cdb_, max_cdb_);
    long below = min_, above = max_;
    snd_mixer_selem_ask_playback_dB_vol(elem_, target, -1, &below);
    snd_mixer_selem_ask_playback_dB_vol(elem_, target, 1, &above);
    long below_cdb = 0, above_cdb = 0;
    if (snd_mixer_selem_ask_playback_vol_dB(elem_, below, &below_cdb) < 0) return above;
    if (snd_mixer_selem_ask_playback_vol_dB(elem_, above, &above_cdb) < 0) return below;
    return std::labs(target - below_cdb) <= std::labs(above_cdb - target) ? below : above;
}

uint8_t VolumeControl::volume_for_raw(long raw) {
    if (raw <= min_) return 0;
    long cdb = 0;
    if (!has_db_ || snd_mixer_selem_ask_playback_vol_dB(elem_, raw, &cdb) < 0)
        return raw_to_sendspin(raw, min_, max_);
    return centidb_to_sendspin(cdb, min_cdb_, max_cdb_);
}

void VolumeControl::set_sendspin_volume(uint8_t v) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!elem_) return;
    long raw = raw_for_volume(v);
    snd_mixer_selem_set_playback_volume_all(elem_, raw);
    // Several volumes can share a raw step; remembering the step (not v) keeps
    // the poll from echoing a neighbouring volume back to the server.
    last_raw_ = raw;
}

void VolumeControl::set_muted(bool muted) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!elem_) return;
    if (snd_mixer_selem_has_playback_switch(elem_)) {
        snd_mixer_selem_set_playback_switch_all(elem_, muted ? 0 : 1);
        last_switch_ = muted ? 0 : 1;
    }
}

std::optional<uint8_t> VolumeControl::poll_volume_change() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!elem_) return std::nullopt;
    snd_mixer_handle_events(handle_);
    long raw = 0;
    if (snd_mixer_selem_get_playback_volume(elem_, SND_MIXER_SCHN_FRONT_LEFT, &raw) < 0)
        return std::nullopt;
    if (raw == last_raw_) return std::nullopt;
    last_raw_ = raw;
    return volume_for_raw(raw);
}

std::optional<bool> VolumeControl::poll_mute_change() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!elem_ || !snd_mixer_selem_has_playback_switch(elem_)) return std::nullopt;
    snd_mixer_handle_events(handle_);
    int sw = 1;
    if (snd_mixer_selem_get_playback_switch(elem_, SND_MIXER_SCHN_FRONT_LEFT, &sw) < 0)
        return std::nullopt;
    sw = sw ? 1 : 0;
    if (sw == last_switch_) return std::nullopt;
    last_switch_ = sw;
    return sw == 0;
}
