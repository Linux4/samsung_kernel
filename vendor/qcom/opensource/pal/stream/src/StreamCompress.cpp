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

#define LOG_TAG "PAL: StreamCompress"
#include "StreamCompress.h"
#include "Session.h"
#include "SessionAlsaPcm.h"
#include "SessionAlsaCompress.h"
#include "ResourceManager.h"
#include "Device.h"
#include <unistd.h>
#include <chrono>

#define COMPRESS_OFFLOAD_FRAGMENT_SIZE (32 * 1024)
#define COMPRESS_OFFLOAD_NUM_FRAGMENTS 4

std::condition_variable cvPause;

static void handleSessionCallBack(uint64_t hdl, uint32_t event_id, void *data,
                                  uint32_t event_size)
{
    Stream *s = NULL;
    pal_stream_callback cb;

    PAL_DBG(LOG_TAG,"Event id %x ", event_id);
    if (event_id == EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE) {
        PAL_DBG(LOG_TAG,"Pause Done");
        cvPause.notify_all();
    }
    else {
        s = reinterpret_cast<Stream *>(hdl);
        if (s->getCallBack(&cb) == 0)
            cb(reinterpret_cast<pal_stream_handle_t *>(s), event_id, (uint32_t *)data,
               event_size, s->cookie);
    }
}

StreamCompress::StreamCompress(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
                               const uint32_t no_of_devices, const struct modifier_kv *modifiers,
                               const uint32_t no_of_modifiers, const std::shared_ptr<ResourceManager> rm)
{
    mStreamMutex.lock();

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Sound card offline, can not create stream");
        usleep(SSR_RECOVERY);
        mStreamMutex.unlock();
        throw std::runtime_error("Sound card offline");
    }

    std::shared_ptr<Device> dev = nullptr;
    bool isDeviceConfigUpdated = false;

    session = nullptr;
    mGainLevel = -1;
    inBufSize = COMPRESS_OFFLOAD_FRAGMENT_SIZE;
    outBufSize = COMPRESS_OFFLOAD_FRAGMENT_SIZE;
    inBufCount = COMPRESS_OFFLOAD_NUM_FRAGMENTS;
    outBufCount = COMPRESS_OFFLOAD_NUM_FRAGMENTS;
    mDevices.clear();
    mPalDevice.clear();
    PAL_VERBOSE(LOG_TAG,"enter");

    //TBD handle modifiers later
    mNoOfModifiers = 0; //no_of_modifiers;
    mModifiers = (struct modifier_kv *) (NULL);
    std::ignore = modifiers;
    std::ignore = no_of_modifiers;
    currentState = STREAM_IDLE;

    // Setting default volume to unity
    mVolumeData = (struct pal_volume_data *)calloc(1, sizeof(struct pal_volume_data)
                          + sizeof(struct pal_channel_vol_kv));
    if (!mVolumeData) {
        PAL_ERR(LOG_TAG, "malloc for volume data failed");
        mStreamMutex.unlock();
        throw std::runtime_error("failed to malloc for volume data");
    }
    mVolumeData->no_of_volpair = 1;
    mVolumeData->volume_pair[0].channel_mask = 0x03;
    mVolumeData->volume_pair[0].vol = 1.0f;

    mStreamAttr = (struct pal_stream_attributes *)calloc(1, sizeof(struct pal_stream_attributes));
    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "malloc for stream attributes failed");
        mStreamMutex.unlock();
        throw std::runtime_error("failed to malloc for stream attributes");
    }
    ar_mem_cpy(mStreamAttr, sizeof(pal_stream_attributes), sattr, sizeof(pal_stream_attributes));
    PAL_VERBOSE(LOG_TAG, "Create new compress session");

    session = Session::makeSession(rm, sattr);
    if (session == NULL){
       PAL_ERR(LOG_TAG,"session (compress) creation failed");
       free(mStreamAttr);
       mStreamMutex.unlock();
       throw std::runtime_error("failed to create session object");
    }

    session->registerCallBack(handleSessionCallBack, (uint64_t)this);
    PAL_VERBOSE(LOG_TAG,"Create new Devices with no_of_devices - %d", no_of_devices);
    for (uint32_t i = 0; i < no_of_devices; i++) {
        dev = Device::getInstance((struct pal_device *)&dattr[i] , rm);
        if (dev == nullptr) {
            PAL_ERR(LOG_TAG, "Device creation is failed");
            free(mStreamAttr);
            mStreamMutex.unlock();
            throw std::runtime_error("failed to create device object");
        }
        mStreamMutex.unlock();
        isDeviceConfigUpdated = rm->updateDeviceConfig(&dev, &dattr[i], sattr);
        mStreamMutex.lock();

        if (isDeviceConfigUpdated)
            PAL_VERBOSE(LOG_TAG, "Device config updated");

        mDevices.push_back(dev);
        mPalDevice.push_back(dattr[i]);
        dev = nullptr;
    }
    mStreamMutex.unlock();
    rm->registerStream(this);
    PAL_VERBOSE(LOG_TAG,"exit, state %d", currentState);
}

int32_t StreamCompress::open()
{
    int32_t status = 0;
    mStreamMutex.lock();

    PAL_DBG(LOG_TAG,"Enter, session handle - %p device count - %zu state %d",
                       session, mDevices.size(), currentState);
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        status = -EIO;
        PAL_ERR(LOG_TAG, "Sound card offline, can not open stream");
        usleep(SSR_RECOVERY);
        goto exit;
    }

    if (currentState == STREAM_IDLE) {
        status = session->open(this);
        if (0 != status) {
           PAL_ERR(LOG_TAG,"session open failed with status %d", status);
           goto exit;
        }
        PAL_VERBOSE(LOG_TAG, "session open successful");
        for (int32_t i=0; i < mDevices.size(); i++) {
            PAL_DBG(LOG_TAG, "device %d name %s, going to open",
                mDevices[i]->getSndDeviceId(), mDevices[i]->getPALDeviceName().c_str());

            status = mDevices[i]->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG,"device open failed with status %d", status);
                goto exit;
             }
        }
        currentState = STREAM_INIT;
        PAL_VERBOSE(LOG_TAG,"device open successful");
        PAL_VERBOSE(LOG_TAG,"exit stream compress opened, state %d", currentState);
    } else if (currentState == STREAM_INIT) {
        PAL_INFO(LOG_TAG, "Stream is already opened, state %d", currentState);
        goto exit;
    } else {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Stream is not in correct state, state %d", currentState);
        goto exit;
    }
exit:
    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int32_t StreamCompress::close()
{
    int32_t status = 0;

    mStreamMutex.lock();
    if (currentState == STREAM_IDLE) {
        PAL_INFO(LOG_TAG, "Stream is already closed");
        mStreamMutex.unlock();
        return status;
    }

    PAL_DBG(LOG_TAG,"Enter, session handle - %p mDevices count - %zu state %d",
                session, mDevices.size(), currentState);
    if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        mStreamMutex.unlock();
        status = stop();
        if (0 != status) {
            PAL_ERR(LOG_TAG,"stop failed with status %d", status);
        }
        mStreamMutex.lock();
    }
    rm->lockGraph();
    status = session->close(this);
    rm->unlockGraph();
    if (0 != status) {
        PAL_ERR(LOG_TAG,"session close failed with status %d", status);
    }

    for (int32_t i = 0; i < mDevices.size(); i++) {
        PAL_INFO(LOG_TAG, "device %d (%s), going to close",
            mDevices[i]->getSndDeviceId(), mDevices[i]->getPALDeviceName().c_str());

        status = mDevices[i]->close();
        if (0 != status) {
            PAL_ERR(LOG_TAG,"device close failed with status %d", status);
        }
    }
    PAL_VERBOSE(LOG_TAG,"closed the devices successfully");
    currentState = STREAM_IDLE;

    mStreamMutex.unlock();

    PAL_DBG(LOG_TAG,"Exit status: %d",status);
    return status;
}

StreamCompress::~StreamCompress()
{
    rm->resetStreamInstanceID(this);
    rm->deregisterStream(this);

    if (mStreamAttr) {
        free(mStreamAttr);
        mStreamAttr = (struct pal_stream_attributes *)NULL;
    }

    if(mVolumeData)  {
        free(mVolumeData);
        mVolumeData = (struct pal_volume_data *)NULL;
    }

    /*switch back to proper config if there is a concurrency and device is still running*/
    for (int32_t i=0; i < mDevices.size(); i++)
        rm->restoreDevice(mDevices[i]);

    mDevices.clear();
    mPalDevice.clear();
    if (session) {
        delete session;
        session = nullptr;
    }
}

int32_t StreamCompress::stop()
{
    int32_t status = 0;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG,"Enter. state %d session handle - %p mStreamAttr->direction %d",
                currentState, session, mStreamAttr->direction);
    if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        for (int i = 0; i < mDevices.size(); i++) {
            rm->deregisterDevice(mDevices[i], this);
        }
        switch (mStreamAttr->direction) {
        case PAL_AUDIO_OUTPUT:
            PAL_VERBOSE(LOG_TAG,"In PAL_AUDIO_OUTPUT case, device count - %zu", mDevices.size());

            rm->lockGraph();
            status = session->stop(this);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Rx session stop failed with status %d",status);
            }
            PAL_VERBOSE(LOG_TAG,"session stop successful");

            for (int32_t i = 0; i < mDevices.size(); i++) {
                PAL_ERR(LOG_TAG, "device %d name %s, going to stop",
                    mDevices[i]->getSndDeviceId(), mDevices[i]->getPALDeviceName().c_str());

                status = mDevices[i]->stop();
                if (0 != status) {
                    PAL_ERR(LOG_TAG,"Rx device stop failed with status %d",status);
                }
            }
            rm->unlockGraph();
            PAL_VERBOSE(LOG_TAG,"devices stop successful");
            break;
        default:
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "invalid direction %d", mStreamAttr->direction);
            break;
        }
        currentState = STREAM_STOPPED;
    } else if (currentState == STREAM_STOPPED || currentState == STREAM_IDLE) {
        PAL_INFO(LOG_TAG, "Stream is already stopped, state %d", currentState);
        goto exit;
    } else {
        PAL_ERR(LOG_TAG, "Stream should be in STREAM_STARTED/STREAM_PAUSED state, state %d",
                currentState);
        status = -EINVAL;
        goto exit;
    }
exit:
    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int32_t StreamCompress::start()
{
    int32_t status = 0;
    bool a2dpSuspend = false;

    mStreamMutex.lock();

    PAL_VERBOSE(LOG_TAG,"Enter, session handle - %p mStreamAttr->direction - %d",
                    session, mStreamAttr->direction);

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Sound card offline");
        status = -EIO;
        goto exit;
    }

    if (currentState == STREAM_INIT || currentState == STREAM_STOPPED) {
        switch (mStreamAttr->direction) {
        case PAL_AUDIO_OUTPUT:
            PAL_VERBOSE(LOG_TAG, "Inside PAL_AUDIO_OUTPUT device count - %zu", mDevices.size());

            // handle scenario where BT device is not ready
            status = handleBTDeviceNotReady(a2dpSuspend);
            if (0 != status)
                goto exit;

            rm->lockGraph();
            for (int32_t i=0; i < mDevices.size(); i++) {
                PAL_ERR(LOG_TAG, "device %d name %s, going to start",
                    mDevices[i]->getSndDeviceId(), mDevices[i]->getPALDeviceName().c_str());

                status = mDevices[i]->start();
                if (0 != status) {
                    PAL_ERR(LOG_TAG,"Rx device start failed with status %d", status);
                    rm->unlockGraph();
                    goto exit;
                }
            }
            PAL_VERBOSE(LOG_TAG,"devices started successfully");

            status = session->prepare(this);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Rx session prepare is failed with status %d",status);
                rm->unlockGraph();
                goto session_fail;
            }
            PAL_VERBOSE(LOG_TAG,"session prepare successful");

            status = session->start(this);
            if (errno == -ENETRESET) {
                if (rm->cardState != CARD_STATUS_OFFLINE) {
                    PAL_ERR(LOG_TAG, "Sound card offline, informing rm");
                    rm->ssrHandler(CARD_STATUS_OFFLINE);
                }
                status = 0;
                rm->unlockGraph();
                goto session_fail;
            }
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Rx session start is failed with status %d",status);
                rm->unlockGraph();
                goto session_fail;
            }
            PAL_VERBOSE(LOG_TAG, "session start successful");
            rm->unlockGraph();

            if (a2dpSuspend) {
                PAL_DBG(LOG_TAG, "mute the stream on speaker");
                if (!a2dpMuted) {
                    mute_l(true);
                    a2dpMuted = true;
                }
            }
            break;
        default:
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "direction %d not supported for compress streams", mStreamAttr->direction);
            break;
        }
        currentState = STREAM_OPENED;
        goto exit;
    } else if (currentState == STREAM_OPENED) {
        PAL_ERR(LOG_TAG, "Stream in already in started state, state %d", currentState);
        status = 0;
        goto exit;
    } else {
        PAL_ERR(LOG_TAG, "Stream should be in STREAM_INIT/STREAM_PAUSED state, state %d", currentState);
        status = -EINVAL;
        goto exit;
    }
session_fail:
    for (int32_t i=0; i < mDevices.size(); i++)
        status = mDevices[i]->stop();
exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    mStreamMutex.unlock();
    return status;
}

int32_t StreamCompress::prepare()
{
    int32_t status = 0;
    PAL_VERBOSE(LOG_TAG,"Enter, session handle - %p", session);
    mStreamMutex.lock();
    status = session->prepare(this);
    if (status)
       PAL_ERR(LOG_TAG,"session prepare failed with status = %d", status);

    mStreamMutex.unlock();
    PAL_VERBOSE(LOG_TAG,"Exit, status - %d", status);
    return status;
}

int32_t StreamCompress::setStreamAttributes(struct pal_stream_attributes *sattr)
{
    int32_t status = 0;
    PAL_VERBOSE(LOG_TAG,"start, session handle - %p", session);
    memset(mStreamAttr, 0, sizeof(struct pal_stream_attributes));
    mStreamMutex.lock();
    memcpy (mStreamAttr, sattr, sizeof(struct pal_stream_attributes));
    mStreamMutex.unlock();
    status = session->setConfig(this, MODULE, 0);  //gkv or ckv or tkv need to pass
    if (0 != status) {
       PAL_ERR(LOG_TAG,"session setConfig failed with status %d",status);
       goto exit;
    }
    PAL_VERBOSE(LOG_TAG,"session setConfig successful");
    PAL_VERBOSE(LOG_TAG,"end");
exit:
    return status;
}

int32_t StreamCompress::read(struct pal_buffer * /*buf*/)
{
    return 0;
}

int32_t StreamCompress::write(struct pal_buffer *buf)
{
    int32_t status = 0;
    int32_t size;
    PAL_VERBOSE(LOG_TAG, "Enter. session handle - %p state %d", session,
            currentState);

    mStreamMutex.lock();
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        status = -ENETRESET;
        PAL_ERR(LOG_TAG, "Sound Card offline, can not write, status %d",
                status);
        mStreamMutex.unlock();
        return status;
    }

    // we should allow writes to go through in Open/Start/Pause state as well.
    if ((currentState == STREAM_OPENED) ||
        (currentState == STREAM_STARTED) ||
        (currentState == STREAM_PAUSED)) {
        status = session->write(this, SHMEM_ENDPOINT, buf, &size, 0);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "session write failed with status %d", status);
            if (errno == -ENETRESET && rm->cardState != CARD_STATUS_OFFLINE) {
                PAL_ERR(LOG_TAG, "Sound card offline, informing rm");
                rm->ssrHandler(CARD_STATUS_OFFLINE);
                mStreamMutex.unlock();
                return errno;
            } else if (rm->cardState == CARD_STATUS_OFFLINE) {
                mStreamMutex.unlock();
                return errno;
            } else {
                mStreamMutex.unlock();
                return status;
            }
        }
        if ((currentState != STREAM_STARTED) &&
            (currentState != STREAM_PAUSED)) {
            currentState = STREAM_STARTED;
            // register device only after graph is actually started
            for (int i = 0; i < mDevices.size(); i++) {
                rm->registerDevice(mDevices[i], this);
            }
        }
    } else {
        PAL_ERR(LOG_TAG, "Stream not opened yet, state %d", currentState);
        status = -EINVAL;
        mStreamMutex.unlock();
        return status;
    }

    mStreamMutex.unlock();
    PAL_VERBOSE(LOG_TAG, "Exit. session write successful size - %d", size);
    return size;
}

int32_t StreamCompress::registerCallBack(pal_stream_callback cb, uint64_t cookie)
{
    streamCb = cb;
    this->cookie = cookie;
    return 0;
}

int32_t StreamCompress::getCallBack(pal_stream_callback *cb)
{
    *cb = streamCb;
    return 0;
}

int32_t StreamCompress::getParameters(uint32_t /*param_id*/, void ** /*payload*/)
{
    return 0;
}

int32_t StreamCompress::setParameters(uint32_t param_id, void *payload)
{
    int32_t status = 0;
    pal_param_payload *param_payload = NULL;
    effect_pal_payload_t *effectPalPayload = nullptr;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG, "Enter");
    switch (param_id) {
        case PAL_PARAM_ID_UIEFFECT:
        {
            param_payload = (pal_param_payload *)payload;
            effectPalPayload = (effect_pal_payload_t *)(param_payload->payload);
            if (effectPalPayload->isTKV) {
                status = session->setTKV(this, MODULE, effectPalPayload);
            } else {
                status = session->setParameters(this, effectPalPayload->tag, param_id, payload);
            }
            if (status) {
               PAL_ERR(LOG_TAG, "setParameters %d failed with %d", param_id, status);
            }
            break;
        }
        case PAL_PARAM_ID_DEVICE_ROTATION:
        {
            // Call Session for Setting the parameter.
            if (NULL != session) {
                status = session->setParameters(this, 0,
                                                PAL_PARAM_ID_DEVICE_ROTATION,
                                                payload);
            } else {
                PAL_ERR(LOG_TAG, "Session is null");
                status = -EINVAL;
            }
        }
        break;
        case PAL_PARAM_ID_VOLUME_USING_SET_PARAM:
        {
            status = session->setParameters(this, PAL_PARAM_ID_VOLUME_USING_SET_PARAM,
                                            param_id, payload);
            if (status)
               PAL_ERR(LOG_TAG, "setParam for volume failed with %d",
                       status);
            break;
        }
        default:
            status = session->setParameters(this, 0, param_id, payload);
            break;
    }

    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG, "Exit, session parameter %u set with status %d", param_id, status);
    return status;
}

int32_t StreamCompress::setVolume(struct pal_volume_data *volume)
{
    int32_t status = 0;
    struct volume_set_param_info vol_set_param_info;
    uint8_t volSize = 0;

    PAL_DBG(LOG_TAG, "Enter, session handle - %p", session);
    if (!volume|| volume->no_of_volpair == 0) {
       PAL_ERR(LOG_TAG, "Invalid arguments");
       status = -EINVAL;
       goto exit;
    }

    if (mVolumeData) {
        // if mVolumeDate is already allocated, free it before updating
        free(mVolumeData);
        mVolumeData = (struct pal_volume_data *)NULL;
    }

    mVolumeData = (struct pal_volume_data *)calloc(1, sizeof(struct pal_volume_data) +
                 (sizeof(struct pal_channel_vol_kv) * (volume->no_of_volpair)));
    if (!mVolumeData) {
       PAL_ERR(LOG_TAG, "failed to calloc for volume data");
       status = -ENOMEM;
       goto exit;
    }

    volSize = (sizeof(struct pal_volume_data) +
            (sizeof(struct pal_channel_vol_kv) * (volume->no_of_volpair)));
    memcpy(mVolumeData, volume, volSize);
    for(int32_t i = 0; i < (mVolumeData->no_of_volpair); i++) {
       PAL_VERBOSE(LOG_TAG, "Volume payload mask:%x vol:%f",
               (mVolumeData->volume_pair[i].channel_mask), (mVolumeData->volume_pair[i].vol));
    }
    /* Allow caching of stream volume as part of mVolumeData
     * till the stream_start is not done or if sound card is
     * offline.
     */
    memset(&vol_set_param_info, 0, sizeof(struct volume_set_param_info));
    rm->getVolumeSetParamInfo(&vol_set_param_info);
    if (rm->cardState == CARD_STATUS_ONLINE && currentState != STREAM_IDLE
        && currentState != STREAM_INIT) {
        bool isStreamAvail = (find(vol_set_param_info.streams_.begin(),
                    vol_set_param_info.streams_.end(), mStreamAttr->type) !=
                    vol_set_param_info.streams_.end());
        if (isStreamAvail && vol_set_param_info.isVolumeUsingSetParam) {
            uint8_t *volPayload = new uint8_t[sizeof(pal_param_payload) +
                volSize]();
            pal_param_payload *pld = (pal_param_payload *)volPayload;
            pld->payload_size = sizeof(struct pal_volume_data);
            memcpy(pld->payload, mVolumeData, volSize);
            status = setParameters(PAL_PARAM_ID_VOLUME_USING_SET_PARAM, (void *)pld);
            delete[] volPayload;
        } else {
            status = session->setConfig(this, CALIBRATION, TAG_STREAM_VOLUME);
        }
        if (0 != status) {
           PAL_ERR(LOG_TAG, "session setConfig for VOLUME_TAG failed with status %d",status);
           goto exit;
        }
    }
    PAL_VERBOSE(LOG_TAG, "Volume payload No.of vol pair:%d ch mask:%x gain:%f",
             (volume->no_of_volpair), (volume->volume_pair->channel_mask),(volume->volume_pair->vol));
exit:
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int32_t StreamCompress::mute_l(bool state)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK state %d", session, state);
    status = session->setConfig(this, MODULE, state ? MUTE_TAG : UNMUTE_TAG);
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int32_t StreamCompress::mute(bool state)
{
    int32_t status = 0;

    mStreamMutex.lock();
    status = mute_l(state);
    mStreamMutex.unlock();

    return status;
}

int32_t StreamCompress::pause_l()
{
    int32_t status = 0;
    std::unique_lock<std::mutex> pauseLock(pauseMutex);

    //AF will try to pause the stream during SSR.
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Sound card offline, can not pause, status %d", status);
        isPaused = true;
        return status;
    }

    PAL_DBG(LOG_TAG,"Enter, session handle - %p, state %d",
               session, currentState);

    if (currentState != STREAM_PAUSED) {
        status = session->setConfig(this, MODULE, PAUSE_TAG);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"session setConfig for pause failed with status %d",status);
            goto exit;
        }
        PAL_DBG(LOG_TAG, "Waiting for Pause to complete");
        if (session->isPauseRegistrationDone)
            cvPause.wait_for(pauseLock, std::chrono::microseconds(VOLUME_RAMP_PERIOD));
        else
            usleep(VOLUME_RAMP_PERIOD);
        isPaused = true;
        currentState = STREAM_PAUSED;
        PAL_VERBOSE(LOG_TAG,"session pause successful, state %d", currentState);
    } else {
       PAL_INFO(LOG_TAG, "Stream is already paused");
    }

exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int32_t StreamCompress::pause()
{
    int32_t status = 0;
    status = pause_l();

    return status;
}

int32_t StreamCompress::resume_l()
{
    int32_t status = 0;

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Sound card offline, can not resume, status %d", status);
        return status;
    }

    PAL_DBG(LOG_TAG,"Enter, session handle - %p, state %d",
               session, currentState);

    status = session->setConfig(this, MODULE, RESUME_TAG);
    if (0 != status) {
       PAL_ERR(LOG_TAG,"session setConfig for pause failed with status %d",status);
       goto exit;
    }

    isPaused = false;
    currentState = STREAM_STARTED;
    PAL_VERBOSE(LOG_TAG,"session resume successful, state %d", currentState);

exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int32_t StreamCompress::resume()
{
    int32_t status = 0;
    status = resume_l();

    return status;
}

int32_t StreamCompress::drain(pal_drain_type_t type)
{
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Sound card offline or session is null");
        return -EINVAL;
    }
    return session->drain(type);
}

int32_t StreamCompress::flush()
{
    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (isPaused == false) {
        PAL_DBG(LOG_TAG, "Flush called while stream is not Paused");
        return 0;
    }
    if (currentState == STREAM_STOPPED ||
        currentState == STREAM_IDLE) {
        PAL_ERR(LOG_TAG, "Session already flushed, state %d",
               currentState);
        return 0;
    }
    return session->flush();
}

int32_t StreamCompress::isSampleRateSupported(uint32_t sampleRate)
{
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    // Allow all samplerates for compress streams
    return 0;
}

int32_t StreamCompress::isChannelSupported(uint32_t numChannels)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    switch(numChannels) {
        case CHANNELS_1:
        case CHANNELS_2:
        case CHANNELS_3:
        case CHANNELS_4:
        case CHANNELS_5:
        case CHANNELS_5_1:
        case CHANNELS_7:
        case CHANNELS_8:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "channels not supported %d rc %d", numChannels, rc);
            break;
    }
    return rc;
}

int32_t StreamCompress::isBitWidthSupported(uint32_t bitWidth)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    switch(bitWidth) {
        case BITWIDTH_16:
        case BITWIDTH_24:
        case BITWIDTH_32:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "bit width not supported %d rc %d", bitWidth, rc);
            break;
    }
    return rc;
}

int32_t StreamCompress::addRemoveEffect(pal_audio_effect_t /*effect*/, bool /*enable*/)
{
    PAL_ERR(LOG_TAG, "Function not supported");
    return -ENOSYS;
}

int32_t StreamCompress::setECRef(std::shared_ptr<Device> dev, bool is_enable)
{
    int32_t status = 0;

    mStreamMutex.lock();
    status = setECRef_l(dev, is_enable);
    mStreamMutex.unlock();

    return status;
}

int32_t StreamCompress::setECRef_l(std::shared_ptr<Device> dev, bool is_enable)
{
    int32_t status = 0;

    if (!session)
        return -EINVAL;

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK", session);

    status = session->setECRef(this, dev, is_enable);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to set ec ref in session");
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamCompress::ssrDownHandler()
{
    int status = 0;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK state %d", session, currentState);

    if (currentState == STREAM_INIT || currentState == STREAM_STOPPED || currentState == STREAM_OPENED) {
        mStreamMutex.unlock();
        status = close();
        if (status) {
           PAL_ERR(LOG_TAG, "stream close failed. status %d", status);
            goto exit;
        }
    } else if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        mStreamMutex.unlock();
        status = stop();
        if (status)
            PAL_ERR(LOG_TAG, "stream stop failed. status %d",  status);
        status = close();
        if (status) {
            PAL_ERR(LOG_TAG, "session close failed. status %d", status);
            goto exit;
        }
    } else {
       mStreamMutex.unlock();
       PAL_ERR(LOG_TAG, "stream state is %d, nothing to handle", currentState);
       goto exit;
    }

exit :
    currentState = STREAM_IDLE;
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamCompress::ssrUpHandler()
{
    /* As Compressed session will be completely closed during SSR down,
      * during SSR up, a new session either PCM DB or Compressed Session
      * will be started.
      */
    return 0;
}
