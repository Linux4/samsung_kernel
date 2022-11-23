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

#ifndef SESSION_H
#define SESSION_H

#include "PayloadBuilder.h"
#include "PalDefs.h"
#include <mutex>
#include <algorithm>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <errno.h>
#include "PalCommon.h"
#include "Device.h"



typedef enum {
    GRAPH = 0,
    MODULE ,
    CALIBRATION,
    CODEC_DMA_CONFIG,
    MEDIA_CONFIG,
    IN_MEDIA_CONFIG,
    OUT_MEDIA_CONFIG
}configType;

typedef enum {
    SESSION_IDLE,
    SESSION_OPENED,
    SESSION_STARTED,
    SESSION_FLUSHED,
    SESSION_STOPPED,
}sessionState;

#define EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE 0x0800103F

class Stream;
class ResourceManager;
class Session
{
protected:
    void * handle_t;
    std::mutex mutex;
    Session();
    std::shared_ptr<ResourceManager> rm;
    struct mixer *mixer;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEnds;
    std::vector<std::pair<int32_t, std::string>> txAifBackEnds;
    void *customPayload;
    size_t customPayloadSize;
    int updateCustomPayload(void *payload, size_t size);
    int freeCustomPayload(uint8_t **payload, size_t *payloadSize);
    int freeCustomPayload();
    uint32_t eventId;
    void *eventPayload;
    size_t eventPayloadSize;
    bool RegisterForEvents = false;
    Stream *streamHandle;
public:
    bool isPauseRegistrationDone;
    virtual ~Session();
    static Session* makeSession(const std::shared_ptr<ResourceManager>& rm, const struct pal_stream_attributes *sAttr);
    int handleDeviceRotation(Stream *s, pal_speaker_rotation_type rotation_type,
        int device, struct mixer *mixer, PayloadBuilder* builder,
        std::vector<std::pair<int32_t, std::string>> rxAifBackEnds);
    int setSlotMask(const std::shared_ptr<ResourceManager>& rm, struct pal_stream_attributes &sAttr,
            struct pal_device &dAttr, const std::vector<int> &pcmDevIds);
    int configureMFC(const std::shared_ptr<ResourceManager>& rm, struct pal_stream_attributes &sAttr,
            struct pal_device &dAttr, const std::vector<int> &pcmDevIds, const char* intf);
    virtual int open(Stream * s) = 0;
    virtual int prepare(Stream * s) = 0;
    virtual int setConfig(Stream * s, configType type, int tag) = 0;
    virtual int setConfig(Stream * s __unused, configType type __unused, uint32_t tag1 __unused,
            uint32_t tag2 __unused, uint32_t tag3 __unused) {return 0;};
    virtual int setConfig(Stream * s __unused, configType type __unused, int tag __unused, int dir __unused) {return 0;};
    virtual int setTKV(Stream * s __unused, configType type __unused, effect_pal_payload_t *payload __unused) {return 0;};
    //virtual int getConfig(Stream * s) = 0;
    virtual int start(Stream * s) = 0;
    virtual int pause(Stream * s);
    virtual int suspend() {return 0;};
    virtual int resume(Stream * s);
    virtual int stop(Stream * s) = 0;
    virtual int close(Stream * s) = 0;
    virtual int readBufferInit(Stream *s __unused, size_t noOfBuf __unused, size_t bufSize __unused, int flag __unused) {return 0;};
    virtual int writeBufferInit(Stream *s __unused, size_t noOfBuf __unused, size_t bufSize __unused, int flag __unused) {return 0;};
    virtual int read(Stream *s __unused, int tag __unused, struct pal_buffer *buf __unused, int * size __unused) {return 0;};
    virtual int write(Stream *s __unused, int tag __unused, struct pal_buffer *buf __unused, int * size __unused, int flag __unused) {return 0;};
    virtual int getParameters(Stream *s __unused, int tagId __unused, uint32_t param_id __unused, void **payload __unused) {return 0;};
    virtual int setParameters(Stream *s __unused, int tagId __unused, uint32_t param_id __unused, void *payload __unused) {return 0;};
    virtual int registerCallBack(session_callback cb __unused, uint64_t cookie __unused) {return 0;};
    virtual int drain(pal_drain_type_t type __unused) {return 0;};
    virtual int flush() {return 0;};
    virtual void setEventPayload(uint32_t event_id __unused, void *payload __unused, size_t payload_size __unused) {  };
    virtual int getTimestamp(struct pal_session_time *stime __unused) {return 0;};
    /*TODO need to implement connect/disconnect in basecase*/
    virtual int setupSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToCconnect) = 0;
    virtual int connectSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToCconnect) = 0;
    virtual int disconnectSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToDisconnect) = 0;
    virtual int setECRef(Stream *s, std::shared_ptr<Device> rx_dev, bool is_enable) = 0;
    void getSamplerateChannelBitwidthTags(struct pal_media_config *config,
        uint32_t &sr_tag, uint32_t &ch_tag, uint32_t &bitwidth_tag);
    virtual uint32_t getMIID(const char *backendName __unused, uint32_t tagId __unused, uint32_t *miid __unused) { return -EINVAL; }
    int getEffectParameters(Stream *s, effect_pal_payload_t *effectPayload);
    int rwACDBParameters(void *payload, uint32_t sampleRate, bool isParamWrite);
    virtual struct mixer_ctl* getFEMixerCtl(const char *controlName __unused, int *device __unused) {return nullptr;}
    virtual int createMmapBuffer(Stream *s __unused, int32_t min_size_frames __unused,
                                   struct pal_mmap_buffer *info __unused) {return -EINVAL;}
    virtual int GetMmapPosition(Stream *s __unused, struct pal_mmap_position *position __unused) {return -EINVAL;}
    virtual int openGraph(Stream *s __unused) { return 0; }
    virtual int getTagsWithModuleInfo(Stream *s __unused, size_t *size __unused,
                                      uint8_t *payload __unused) {return -EINVAL;}
};

#endif //SESSION_H
