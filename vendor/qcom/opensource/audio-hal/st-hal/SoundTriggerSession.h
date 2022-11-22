/*
 * Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef SOUND_TRIGGER_SESSION_H
#define SOUND_TRIGGER_SESSION_H

#include <mutex>

#include <stdlib.h>
#include <cutils/list.h>
#include <pthread.h>
#include <errno.h>
#include <hardware/sound_trigger.h>
#include <cutils/properties.h>

#include "PalDefs.h"
#include "SoundTriggerPropIntf.h"

typedef enum {
    IDLE,
    LOADED,
    ACTIVE,
    DETECTED,
    BUFFERING,
    STOPPING,
    STOPPED,
} session_state_t;

class SoundTriggerSession {
 public:
    ~SoundTriggerSession() {}
    SoundTriggerSession(sound_model_handle_t handle,
        audio_hw_call_back_t callback);
    int LoadSoundModel(struct sound_trigger_sound_model *sound_model);
    int UnloadSoundModel();
    int StartRecognition(const struct sound_trigger_recognition_config *config,
        recognition_callback_t callback, void *cookie, uint32_t version);
    int StopRecognition();
    sound_model_handle_t GetSoundModelHandle();
    int GetCaptureHandle();
    int GetModuleVersion(char version[]);
    void *GetCookie();
    void GetRecognitionCallback(recognition_callback_t *callback);

 protected:
    int OpenPALStream(pal_stream_type_t stream_type);
    bool IsACDSoundModel(struct sound_trigger_sound_model *sound_model);
    void RegisterHalEvent(bool is_register);
    static int pal_callback(pal_stream_handle_t *stream_handle,
        uint32_t event_id, uint32_t *event_data,
        uint32_t event_size, uint64_t cookie);
    int StopRecognition_l();

    session_state_t state_;
    sound_model_handle_t sm_handle_;
    pal_stream_handle_t *pal_handle_;
    recognition_callback_t rec_callback_;
    pal_param_payload *rec_config_payload_;
    struct pal_st_recognition_config *rec_config_;
    audio_hw_call_back_t hal_callback_;
    void *cookie_;
    std::mutex ses_mutex_;
};

#endif  // SOUND_TRIGGER_SESSION_H