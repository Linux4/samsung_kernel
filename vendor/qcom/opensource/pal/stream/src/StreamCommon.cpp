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

#define LOG_TAG "PAL: StreamCommon"
#define RXDIR 0
#define TXDIR 1

#include "StreamCommon.h"
#include "Session.h"
#include "kvh2xml.h"
#include "SessionAlsaPcm.h"
#include "ResourceManager.h"
#include "Device.h"
#include <unistd.h>

StreamCommon::StreamCommon(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
                    const uint32_t no_of_devices, const struct modifier_kv *modifiers,
                    const uint32_t no_of_modifiers, const std::shared_ptr<ResourceManager> rm)
{
    mStreamMutex.lock();
    uint32_t in_channels = 0, out_channels = 0;
    uint32_t attribute_size = 0;

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Error:Sound card offline, can not create stream");
        usleep(SSR_RECOVERY);
        mStreamMutex.unlock();
        throw std::runtime_error("Sound card offline");
    }

    session = NULL;
    mGainLevel = -1;
    std::shared_ptr<Device> dev = nullptr;
    mStreamAttr = (struct pal_stream_attributes *)nullptr;
    inBufSize = BUF_SIZE_CAPTURE;
    outBufSize = BUF_SIZE_PLAYBACK;
    inBufCount = NO_OF_BUF;
    outBufCount = NO_OF_BUF;
    mDevices.clear();
    mPalDevice.clear();
    currentState = STREAM_IDLE;
    //Modify cached values only at time of SSR down.
    cachedState = STREAM_IDLE;
    bool isDeviceConfigUpdated = false;

    PAL_DBG(LOG_TAG, "Enter");

    //TBD handle modifiers later
    mNoOfModifiers = 0; //no_of_modifiers;
    mModifiers = (struct modifier_kv *) (NULL);
    std::ignore = modifiers;
    std::ignore = no_of_modifiers;

    if (!sattr) {
        PAL_ERR(LOG_TAG,"Error:invalid arguments");
        mStreamMutex.unlock();
        throw std::runtime_error("invalid arguments");
    }

    attribute_size = sizeof(struct pal_stream_attributes);
    mStreamAttr = (struct pal_stream_attributes *) calloc(1, attribute_size);
    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "Error:malloc for stream attributes failed %s", strerror(errno));
        mStreamMutex.unlock();
        throw std::runtime_error("failed to malloc for stream attributes");
    }

    memcpy(mStreamAttr, sattr, sizeof(pal_stream_attributes));

    if (mStreamAttr->in_media_config.ch_info.channels > PAL_MAX_CHANNELS_SUPPORTED) {
        PAL_ERR(LOG_TAG,"Error:in_channels is invalid %d", in_channels);
        mStreamAttr->in_media_config.ch_info.channels = PAL_MAX_CHANNELS_SUPPORTED;
    }
    if (mStreamAttr->out_media_config.ch_info.channels > PAL_MAX_CHANNELS_SUPPORTED) {
        PAL_ERR(LOG_TAG,"Error:out_channels is invalid %d", out_channels);
        mStreamAttr->out_media_config.ch_info.channels = PAL_MAX_CHANNELS_SUPPORTED;
    }

    PAL_VERBOSE(LOG_TAG, "Create new Session for stream type %d", sattr->type);
    session = Session::makeSession(rm, sattr);
    if (!session) {
        PAL_ERR(LOG_TAG, "Error:session creation failed");
        free(mStreamAttr);
        mStreamMutex.unlock();
        throw std::runtime_error("failed to create session object");
    }

    PAL_VERBOSE(LOG_TAG, "Create new Devices with no_of_devices - %d", no_of_devices);
    for (int i = 0; i < no_of_devices; i++) {
        //Check with RM if the configuration given can work or not
        //for e.g., if incoming stream needs 24 bit device thats also
        //being used by another stream, then the other stream should route

        dev = Device::getInstance((struct pal_device *)&dattr[i] , rm);
        if (!dev) {
            PAL_ERR(LOG_TAG, "Error:Device creation failed");
            free(mStreamAttr);

            //TBD::free session too
            mStreamMutex.unlock();
            throw std::runtime_error("failed to create device object");
        }
        mStreamMutex.unlock();
        isDeviceConfigUpdated = rm->updateDeviceConfig(&dev, &dattr[i], sattr);
        mStreamMutex.lock();

        if (isDeviceConfigUpdated)
            PAL_VERBOSE(LOG_TAG, "Device config updated");

        /* Create only update device attributes first time so update here using set*/
        /* this will have issues if same device is being currently used by different stream */
        mDevices.push_back(dev);
    }

    if (dattr) {
        for (int i=0; i < no_of_devices; i++) {
            mPalDevice.push_back(dattr[i]);
        }
    }

    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG, "Exit. state %d", currentState);
    return;
}

StreamCommon::~StreamCommon()
{
    PAL_DBG(LOG_TAG, "Enter");
    cachedState = STREAM_IDLE;
    if (mStreamAttr) {
        free(mStreamAttr);
        mStreamAttr = (struct pal_stream_attributes *)NULL;
    }

    /*switch back to proper config if there is a concurrency and device is still running*/
    for (int32_t i=0; i < mDevices.size(); i++)
        rm->restoreDevice(mDevices[i]);

    mDevices.clear();
    mPalDevice.clear();
    delete session;
    session = nullptr;
    PAL_DBG(LOG_TAG, "Exit");
}

int32_t  StreamCommon::open()
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK device count - %zu", session,
            mDevices.size());

    mStreamMutex.lock();
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Error:Sound card offline, can not open stream");
        usleep(SSR_RECOVERY);
        status = -EIO;
        goto exit;
    }

    if (currentState == STREAM_IDLE) {
        status = session->open(this);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:session open failed with status %d", status);
            goto exit;
        }
        PAL_VERBOSE(LOG_TAG, "session open successful");

        for (int32_t i = 0; i < mDevices.size(); i++) {
            status = mDevices[i]->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Error:device open failed with status %d", status);
                goto exit;
            }
        }
        currentState = STREAM_INIT;
        PAL_DBG(LOG_TAG, "streamLL opened. state %d", currentState);
    } else if (currentState == STREAM_INIT) {
        PAL_INFO(LOG_TAG, "Stream is already opened, state %d", currentState);
        status = 0;
        goto exit;
    } else {
        PAL_ERR(LOG_TAG, "Error:Stream is not in correct state %d", currentState);
        //TBD : which error code to return here.
        status = -EINVAL;
        goto exit;
    }
exit:
    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG, "Exit ret %d", status)
    return status;
}

//TBD: move this to Stream, why duplicate code?
int32_t  StreamCommon::close()
{
    int32_t status = 0;
    mStreamMutex.lock();

    if (currentState == STREAM_IDLE) {
        PAL_INFO(LOG_TAG, "Stream is already closed");
        mStreamMutex.unlock();
        return status;
    }

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK device count - %zu stream_type - %d state %d",
             session, mDevices.size(), mStreamAttr->type, currentState);

    if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        mStreamMutex.unlock();
        status = stop();
        if (0 != status)
            PAL_ERR(LOG_TAG, "Error:stream stop failed. status %d",  status);
        mStreamMutex.lock();
    }

    rm->lockGraph();
    status = session->close(this);
    rm->unlockGraph();
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:session close failed with status %d", status);
    }

    for (int32_t i = 0; i < mDevices.size(); i++) {
        status = mDevices[i]->close();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:device close is failed with status %d", status);
        }
    }
    PAL_VERBOSE(LOG_TAG, "closed the devices successfully");
    currentState = STREAM_IDLE;

    mStreamMutex.unlock();

    PAL_DBG(LOG_TAG, "Exit. closed the stream successfully %d status %d",
             currentState, status);
    return status;
}

//TBD: move this to Stream, why duplicate code?
int32_t StreamCommon::start()
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter. session handle - %pK mStreamAttr->direction - %d state %d",
            session, mStreamAttr->direction, currentState);

    mStreamMutex.lock();
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        cachedState = STREAM_STARTED;
        PAL_ERR(LOG_TAG, "Error:Sound card offline. Update the cached state %d",
                cachedState);
        goto exit;
    }

    if (currentState == STREAM_INIT || currentState == STREAM_STOPPED) {
        rm->lockGraph();
        status = start_device();
        if (0 != status) {
            rm->unlockGraph();
            goto exit;
        }
        PAL_VERBOSE(LOG_TAG, "device started successfully");
        status = startSession();
        if (0 != status) {
            rm->unlockGraph();
            goto exit;
        }
        rm->unlockGraph();
        PAL_VERBOSE(LOG_TAG, "session start successful");

        for (int i = 0; i < mDevices.size(); i++) {
            rm->registerDevice(mDevices[i], this);
        }
        /*pcm_open and pcm_start done at once here,
         *so directly jump to STREAM_STARTED state.
         */
        currentState = STREAM_STARTED;
    } else if (currentState == STREAM_STARTED) {
        PAL_INFO(LOG_TAG, "Stream already started, state %d", currentState);
    } else {
        PAL_ERR(LOG_TAG, "Error:Stream is not opened yet");
        status = -EINVAL;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit. state %d", currentState);
    mStreamMutex.unlock();
    return status;
}

int32_t StreamCommon::start_device()
{
    int32_t status = 0;
    for (int32_t i=0; i < mDevices.size(); i++) {
         status = mDevices[i]->start();
         if (0 != status) {
             PAL_ERR(LOG_TAG, "Error:%s device start is failed with status %d",
                     GET_DIR_STR(mStreamAttr->direction), status);
         }
    }
    return status;
}

int32_t StreamCommon::startSession()
{
    int32_t status = 0, devStatus = 0;
    status = session->prepare(this);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:%s session prepare is failed with status %d",
                    GET_DIR_STR(mStreamAttr->direction), status);
        goto session_fail;
    }
    PAL_VERBOSE(LOG_TAG, "session prepare successful");

    status = session->start(this);
    if (errno == -ENETRESET) {
        if (rm->cardState != CARD_STATUS_OFFLINE) {
            PAL_ERR(LOG_TAG, "Error:Sound card offline, informing RM");
            rm->ssrHandler(CARD_STATUS_OFFLINE);
        }
        cachedState = STREAM_STARTED;
        status = 0;
        goto session_fail;
    }
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:%s session start is failed with status %d",
                    GET_DIR_STR(mStreamAttr->direction), status);
        goto session_fail;
    }
    goto exit;

session_fail:
    for (int32_t i=0; i < mDevices.size(); i++) {
        devStatus = mDevices[i]->stop();
        if (devStatus)
            status = devStatus;
        rm->deregisterDevice(mDevices[i], this);
    }
exit:
    return status;
}

//TBD: move this to Stream, why duplicate code?
int32_t StreamCommon::stop()
{
    int32_t status = 0;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK mStreamAttr->direction - %d state %d",
                session, mStreamAttr->direction, currentState);

    if (currentState == STREAM_STARTED || currentState == STREAM_PAUSED) {
        for (int i = 0; i < mDevices.size(); i++) {
            rm->deregisterDevice(mDevices[i], this);
        }
        PAL_VERBOSE(LOG_TAG, "In %s, device count - %zu",
                    GET_DIR_STR(mStreamAttr->direction), mDevices.size());

        rm->lockGraph();
        status = session->stop(this);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:%s session stop failed with status %d",
                    GET_DIR_STR(mStreamAttr->direction), status);
        }
        PAL_VERBOSE(LOG_TAG, "session stop successful");
        for (int32_t i=0; i < mDevices.size(); i++) {
             status = mDevices[i]->stop();
             if (0 != status) {
                 PAL_ERR(LOG_TAG, "Error:%s device stop failed with status %d",
                         GET_DIR_STR(mStreamAttr->direction), status);
             }
        }
        rm->unlockGraph();
        PAL_VERBOSE(LOG_TAG, "devices stop successful");
        currentState = STREAM_STOPPED;
    } else if (currentState == STREAM_STOPPED || currentState == STREAM_IDLE) {
        PAL_INFO(LOG_TAG, "Stream is already in Stopped state %d", currentState);
    } else {
        PAL_ERR(LOG_TAG, "Error:Stream should be in start/pause state, %d", currentState);
        status = -EINVAL;
    }
    PAL_DBG(LOG_TAG, "Exit. status %d, state %d", status, currentState);

    mStreamMutex.unlock();
    return status;
}

//TBD: move this to Stream, why duplicate code?
int32_t  StreamCommon::setVolume(struct pal_volume_data *volume)
{
    int32_t status = 0;
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK", session);
    if (!volume) {
        PAL_ERR(LOG_TAG, "Invalid volume parameter");
        status = -EINVAL;
        goto exit;
    }

    if (volume->no_of_volpair == 0) {
        PAL_ERR(LOG_TAG, "Error no of vol pair is %d", (volume->no_of_volpair));
        status = -EINVAL;
        goto exit;
    }

    /*if already allocated free and reallocate */
    if (mVolumeData) {
        free(mVolumeData);
    }

    mVolumeData = (struct pal_volume_data *)calloc(1, (sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) * (volume->no_of_volpair))));
    if (!mVolumeData) {
        status = -ENOMEM;
        PAL_ERR(LOG_TAG, "mVolumeData malloc failed %s", strerror(errno));
        goto exit;
    }

    //mStreamMutex.lock();
    ar_mem_cpy (mVolumeData, (sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) *
                      (volume->no_of_volpair))), volume, (sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) *
                      (volume->no_of_volpair))));
    //mStreamMutex.unlock();
    for(int32_t i=0; i < (mVolumeData->no_of_volpair); i++) {
        PAL_INFO(LOG_TAG, "Volume payload mask:%x vol:%f",
                      (mVolumeData->volume_pair[i].channel_mask), (mVolumeData->volume_pair[i].vol));
    }
    /* Allow caching of stream volume as part of mVolumeData
     * till the pcm_open is not done or if sound card is
     * offline.
     */
    if (rm->cardState == CARD_STATUS_ONLINE && currentState != STREAM_IDLE
        && currentState != STREAM_INIT) {
        status = session->setConfig(this, CALIBRATION, TAG_STREAM_VOLUME);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "session setConfig for VOLUME_TAG failed with status %d",
                    status);
            goto exit;
        }
    }
    PAL_DBG(LOG_TAG, "Exit. Volume payload No.of vol pair:%d ch mask:%x gain:%f",
                      (volume->no_of_volpair), (volume->volume_pair->channel_mask),
                      (volume->volume_pair->vol));
exit:
    return status;
}

int32_t  StreamCommon::registerCallBack(pal_stream_callback cb, uint64_t cookie)
{
    callback_ = cb;
    cookie_ = cookie;

    PAL_VERBOSE(LOG_TAG, "callback_ = %pK", callback_);

    return 0;
}

int32_t StreamCommon::getTagsWithModuleInfo(size_t *size, uint8_t *payload)
{
    int32_t status = -EINVAL;

    PAL_DBG(LOG_TAG, "Enter");
    if (!payload) {
        PAL_ERR(LOG_TAG, "payload is NULL");
        goto exit;
    }

    if (session)
        status = session->getTagsWithModuleInfo(this, size, payload);
    else
        PAL_ERR(LOG_TAG, "session handle is NULL");

exit:
    return status;
}

int32_t StreamCommon::ssrDownHandler()
{
    int status = 0;

    mStreamMutex.lock();
    /* Updating cached state here only if it's STREAM_IDLE,
     * Otherwise we can assume it is updated by hal thread
     * already.
     */
    if (cachedState == STREAM_IDLE)
        cachedState = currentState;
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK cached State %d",
            session, cachedState);
    switch (currentState) {
    case STREAM_INIT:
    case STREAM_STOPPED:
        mStreamMutex.unlock();
        status = close();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:stream close failed. status %d", status);
            goto exit;
        }
        break;
     case STREAM_STARTED:
     case STREAM_PAUSED:
        mStreamMutex.unlock();
        status = stop();
        if (0 != status)
            PAL_ERR(LOG_TAG, "Error:stream stop failed. status %d",  status);
        status = close();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:stream close failed. status %d", status);
            goto exit;
        }
        break;
     default:
        PAL_ERR(LOG_TAG, "Error:stream state is %d, nothing to handle", currentState);
        mStreamMutex.unlock();
        goto exit;
    }

exit :
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    currentState = STREAM_IDLE;
    return status;
}

int32_t StreamCommon::ssrUpHandler()
{
    int status = 0;

    mStreamMutex.lock();
    PAL_DBG(LOG_TAG, "Enter. session handle - %pK state %d",
            session, cachedState);

    switch (cachedState) {
    case STREAM_INIT:
        mStreamMutex.unlock();
        status = open();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Error:stream open failed. status %d", status);
            goto exit;
        }
        break;
    case STREAM_STARTED:
    case STREAM_PAUSED:
    {
         mStreamMutex.unlock();
         status = open();
         if (0 != status) {
             PAL_ERR(LOG_TAG, "Error:stream open failed. status %d", status);
             goto exit;
         }
         status = start();
         if (0 != status) {
             PAL_ERR(LOG_TAG, "Error:stream start failed. status %d", status);
             goto exit;
         }
         /* For scenario when we get SSR down while handling SSR up,
          * status will be 0, so we need to have this additonal check
          * to keep the cached state as STREAM_STARTED.
          */
         if (currentState != STREAM_STARTED) {
             goto exit;
         }
        }
        break;
     default:
        mStreamMutex.unlock();
        PAL_ERR(LOG_TAG, "Error:stream not in correct state to handle %d", cachedState);
        break;
    }
    cachedState = STREAM_IDLE;
exit :
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}
