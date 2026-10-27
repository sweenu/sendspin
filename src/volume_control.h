// src/volume_control.h
#pragma once
#include <alsa/asoundlib.h>
#include <climits>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

class VolumeControl {
 public:
    VolumeControl(std::string mixer_device, std::string control_name);
    ~VolumeControl();
    bool open();
    void set_sendspin_volume(uint8_t v);
    void set_muted(bool muted);
    // The mixer's volume / mute if it changed since the last poll or set, i.e.
    // a change made outside the daemon. The first poll always reports.
    std::optional<uint8_t> poll_volume_change();
    std::optional<bool> poll_mute_change();

 private:
    std::string device_, control_;
    snd_mixer_t* handle_ = nullptr;
    snd_mixer_elem_t* elem_ = nullptr;
    long min_ = 0, max_ = 0;
    bool has_db_ = false;
    long min_cdb_ = 0, max_cdb_ = 0;
    long last_raw_ = LONG_MIN;   // last raw volume set or reported
    int last_switch_ = -1;       // last playback switch set or reported
    std::mutex mtx_;
    long raw_for_volume(uint8_t v);
    uint8_t volume_for_raw(long raw);
};
