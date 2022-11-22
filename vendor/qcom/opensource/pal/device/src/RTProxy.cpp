/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "PAL: RTProxy"
#define LOG_TAG_OUT "PAL: RTProxyOut"
#include "RTProxy.h"
#include "ResourceManager.h"
#include "Device.h"
#include "kvh2xml.h"

#include "PayloadBuilder.h"
#include "Stream.h"
#include "Session.h"

std::shared_ptr<Device> RTProxy::obj = nullptr;

std::shared_ptr<Device> RTProxy::getObject()
{
    return obj;
}


std::shared_ptr<Device> RTProxy::getInstance(struct pal_device *device,
                                             std::shared_ptr<ResourceManager> Rm)
{
    if (!obj) {
        std::shared_ptr<Device> sp(new RTProxy(device, Rm));
        obj = sp;
    }
    return obj;
}


RTProxy::RTProxy(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
Device(device, Rm)
{
    rm = Rm;
    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));
}

RTProxy::~RTProxy()
{

}

int32_t RTProxy::isSampleRateSupported(uint32_t sampleRate)
{
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    /* Proxy supports all sample rates, accept by default */
    return 0;
}

int32_t RTProxy::isChannelSupported(uint32_t numChannels)
{
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    /* Proxy supports all channels, accept by default */
    return 0;
}

int32_t RTProxy::isBitWidthSupported(uint32_t bitWidth)
{
    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    /* Proxy supports all bitwidths, accept by default */
    return 0;
}


int RTProxy::start() {
    int status = 0;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t ratMiid = 0;
    //struct pal_media_config config;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activestreams;
    std::shared_ptr<Device> dev = nullptr;
    std::string backEndName;
    PayloadBuilder* builder = new PayloadBuilder();

    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;

    rm->getBackendName(mDeviceAttr.id, backEndName);

    dev = Device::getInstance(&deviceAttr, rm);

    status = rm->getActiveStream_l(dev, activestreams);
    if ((0 != status) || (activestreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        status = -EINVAL;
        goto error;
    }
    stream = static_cast<Stream *>(activestreams[0]);
    stream->getAssociatedSession(&session);

    status = session->getMIID(backEndName.c_str(), RAT_RENDER, &ratMiid);
    if (status) {
        PAL_INFO(LOG_TAG,
         "Failed to get tag info %x Skipping RAT Configuration Setup, status = %d",
          RAT_RENDER, status);
        //status = -EINVAL;
        status = 0;
        goto start;
    }

    builder->payloadRATConfig(&paramData, &paramSize, ratMiid, &mDeviceAttr.config);
    if (dev && paramSize) {
        dev->updateCustomPayload(paramData, paramSize);
        if (paramData)
            free(paramData);
        paramData = NULL;
        paramSize = 0;
    } else {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid RAT module param size");
        goto error;
    }
start:
    status = Device::start();
error:
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

std::shared_ptr<Device> RTProxyOut::obj = nullptr;

std::shared_ptr<Device> RTProxyOut::getInstance(struct pal_device *device,
                                                     std::shared_ptr<ResourceManager> Rm)
{
    if (!obj) {
        std::shared_ptr<Device> sp(new RTProxyOut(device, Rm));
        obj = sp;
    }
    return obj;
}

RTProxyOut::RTProxyOut(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
    Device(device, Rm)
{
    rm = Rm;
    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));
}

int32_t RTProxyOut::isSampleRateSupported(uint32_t sampleRate)
{
    PAL_DBG(LOG_TAG,"sampleRate %u", sampleRate);

    /* ProxyOut supports all sample rates, accept by default */
    return 0;
}

int32_t RTProxyOut::isChannelSupported(uint32_t numChannels)
{
    PAL_DBG(LOG_TAG,"numChannels %u", numChannels);

    /* ProxyOut supports all channels */
    return 0;
}

int32_t RTProxyOut::isBitWidthSupported(uint32_t bitWidth)
{
    PAL_DBG(LOG_TAG,"bitWidth %u", bitWidth);

    /* ProxyOut supports all bitWidth configurations */
    return 0;
}

int RTProxyOut::start() {
    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;

    return Device::start();
}

