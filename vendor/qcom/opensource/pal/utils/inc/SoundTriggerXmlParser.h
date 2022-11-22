/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef SOUND_TRIGGER_XML_PARSER_H
#define SOUND_TRIGGER_XML_PARSER_H

#include "PalDefs.h"

#define CAPTURE_PROFILE_PRIORITY_HIGH 1
#define CAPTURE_PROFILE_PRIORITY_LOW -1
#define CAPTURE_PROFILE_PRIORITY_SAME 0

enum StOperatingModes {
    ST_OPERATING_MODE_LOW_POWER,
    ST_OPERATING_MODE_LOW_POWER_NS,
    ST_OPERATING_MODE_HIGH_PERF,
    ST_OPERATING_MODE_HIGH_PERF_NS,
    ST_OPERATING_MODE_HIGH_PERF_AND_CHARGING
};

enum StInputModes {
    ST_INPUT_MODE_HANDSET,
    ST_INPUT_MODE_HEADSET
};

class SoundTriggerXml {
 public:
    virtual void HandleStartTag(const char *tag, const char **attribs) = 0;
    virtual void HandleEndTag(struct xml_userdata *data __unused, const char *tag __unused) {};
    virtual void HandleCharData(const char *data __unused) {};
    virtual ~SoundTriggerXml() {};
};

class CaptureProfile : public SoundTriggerXml {
 public:
    CaptureProfile(std::string name);
    CaptureProfile() = delete;
    CaptureProfile(CaptureProfile &rhs) = delete;
    CaptureProfile & operator=(CaptureProfile &rhs) = delete;

    void HandleStartTag(const char* tag, const char* * attribs) override;

    std::string GetName() const { return name_; }
    pal_device_id_t GetDevId() const { return device_id_; }
    uint32_t GetSampleRate() const { return sample_rate_; }
    uint32_t GetBitWidth() const { return bitwidth_; }
    uint32_t GetChannels() const { return channels_; }
    std::string GetSndName() const { return snd_name_; }
    bool isECRequired() const { return is_ec_req_; }
    void SetSampleRate(uint32_t sample_rate) { sample_rate_ = sample_rate; }
    void SetBitWidth(uint32_t bit_width) { bitwidth_ = bit_width; }
    void SetChannels(uint32_t channels) { channels_ = channels; }
    void SetSndName(std::string snd_name) { snd_name_ = snd_name; }
    std::pair<uint32_t,uint32_t> GetDevicePpKv() const { return device_pp_kv_; }

    int32_t ComparePriority(std::shared_ptr<CaptureProfile> cap_prof);

 private:
    std::string name_;
    pal_device_id_t device_id_;
    uint32_t sample_rate_;
    uint32_t channels_;
    uint32_t bitwidth_;
    std::pair<uint32_t,uint32_t> device_pp_kv_;
    std::string snd_name_;
    bool is_ec_req_;
};
#endif
