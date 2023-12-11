/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef SOUND_TRIGGER_DEVICE_H
#define SOUND_TRIGGER_DEVICE_H

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>

#include <vector>
#include <memory>
#include <mutex>

#include <hardware/sound_trigger.h>
#include <cutils/properties.h>

#include "SoundTriggerSession.h"
#include "SoundTriggerPropIntf.h"

#define AUDIO_HAL_NAME_PREFIX "audio.primary"

#if LINUX_ENABLED
  #ifdef __LP64__
    #define AUDIO_HAL_LIBRARY_PATH1 "/usr/lib64"
    #define AUDIO_HAL_LIBRARY_PATH2 AUDIO_HAL_LIBRARY_PATH1
  #else
    #define AUDIO_HAL_LIBRARY_PATH1 "/usr/lib"
    #define AUDIO_HAL_LIBRARY_PATH2 AUDIO_HAL_LIBRARY_PATH1
  #endif
#else
  #ifdef __LP64__
    #define AUDIO_HAL_LIBRARY_PATH1 "/vendor/lib64/hw"
    #define AUDIO_HAL_LIBRARY_PATH2 "/system/lib64/hw"
  #else
    #define AUDIO_HAL_LIBRARY_PATH1 "/vendor/lib/hw"
    #define AUDIO_HAL_LIBRARY_PATH2 "/system/lib/hw"
  #endif
#endif

class SoundTriggerDevice {
 public:
    SoundTriggerDevice() {}
    ~SoundTriggerDevice() {}
    static std::shared_ptr<SoundTriggerDevice> GetInstance();
    static std::shared_ptr<SoundTriggerDevice> GetInstance(
        const hw_device_t *device);
    static std::shared_ptr<SoundTriggerDevice> GetInstance(
        const struct sound_trigger_hw_device *st_device);
    int Init(hw_device_t **device, const hw_module_t *module);
    int LoadAudioHal();
    int PlatformInit();
    SoundTriggerSession* GetSession(sound_model_handle_t handle);
    int RegisterSession(SoundTriggerSession* session);
    int DeregisterSession(SoundTriggerSession* session);

    audio_hw_call_back_t ahal_callback_;
    volatile int session_id_;
    bool conc_capture_supported_;

 protected:
    static std::shared_ptr<SoundTriggerDevice> stdev_;
    static std::shared_ptr<struct sound_trigger_hw_device> device_;
    struct sound_trigger_properties *hw_properties_;
    std::vector<SoundTriggerSession *> session_list_;
    std::mutex mutex_;
    audio_devices_t available_devices_;
    void *ahal_handle_;
    unsigned int sthal_prop_api_version_;
};

#endif  // SOUND_TRIGGER_DEVICE_H
