#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tinyalsa/asoundlib.h>
#include "PalCommon.h"
#include "PalDefs.h"
#include "../../device/inc/Device.h"
#include "Stream.h"
#include "Session.h"
//#include "SessionAlsaPcm.h"
#include "ResourceManager.h"
#include <agm/agm_api.h>
#include <iostream>
#include <memory>
#include <string>
#include "fsalgo_dsp_intf.h"
struct fs_module_param_data_t
{
    uint32_t miid;
    uint32_t param_id;
    uint32_t param_size;
    uint32_t error_code;
};

#if 0
static int fsm_get_spk_mode(std::shared_ptr<ResourceManager> rm) {
    std::vector<Stream*> Sactive;
    pal_stream_type_t Stype;
    Stream *Astream = NULL;
    struct fsm_monitor *monitor = fsm_get_monitor();
    int ret = 0; uint32_t Spk_Mode = 0;
    std::shared_ptr<Device> dev = nullptr;

    dev = Device::getInstance(&monitor->deviceAttr, rm);
    if (dev) {
        ret = rm->getActiveStream_l(dev, Sactive);
        if ((0 != ret) || (Sactive.size() == 0)) {
            PAL_ERR(LOG_TAG, " no active stream available");
            goto exit;
        }
        Astream = static_cast<Stream *>(Sactive[0]);
        Astream->getStreamType (&Stype);
        PAL_DBG(LOG_TAG, "stream tyep:%d", Stype);
        switch(Stype) {
            case PAL_STREAM_NON_TUNNEL:
                break;
            case PAL_STREAM_VOICE_CALL:
            case PAL_STREAM_VOICE_UI:
                Spk_Mode = SPK_MODE_VOICE;
                break;
            case PAL_STREAM_VOIP_RX:
            case PAL_STREAM_VOIP_TX:
            case PAL_STREAM_VOIP:
                Spk_Mode = SPK_MODE_VOIP;
                break;
            case PAL_STREAM_LOW_LATENCY:
            case PAL_STREAM_DEEP_BUFFER:
            case PAL_STREAM_COMPRESSED:
            case PAL_STREAM_RAW:
            case PAL_STREAM_PROXY:
            case PAL_STREAM_PCM_OFFLOAD:
            case PAL_STREAM_GENERIC:
            case PAL_STREAM_ACD:
            case PAL_STREAM_LOOPBACK:
            case PAL_STREAM_HAPTICS:
            case PAL_STREAM_ULTRASOUND:
            case PAL_STREAM_SENSOR_PCM_DATA:
                Spk_Mode = SPK_MODE_MUSIC;
                break;
            case PAL_STREAM_ULTRA_LOWLATENCY:
                Spk_Mode = SPK_MODE_VOICE;
                break;
            default:
                Spk_Mode = SPK_MODE_MUSIC;
                break;
        }
        PAL_DBG(LOG_TAG, "the spk mode to be set: %d", Spk_Mode);


    }else {
        PAL_ERR(LOG_TAG, "get device instance falied");
        goto exit;
    }

exit:
    return Spk_Mode;
}
static int fsm_dsp_get_ctl_by_pcm_info(struct mixer *mixer, char *pcm_dev_name,
                                            const char *ctl_cmd, struct mixer_ctl **ctl) {
    char *mixer_name = NULL;
    uint32_t ctl_len = 0;

    if ((mixer == NULL) || (pcm_dev_name == NULL) || (ctl_cmd == NULL) || (ctl == NULL)) {
        PAL_ERR(LOG_TAG, "pointer is null");
        return FSM_ERR;
    }

    ctl_len = strlen(pcm_dev_name) + 1 + strlen(ctl_cmd) + 1;
    mixer_name = (char *)calloc(1, ctl_len);
    if (mixer_name == NULL) {
        PAL_ERR(LOG_TAG, "calloc for mixer name failed");
        return FSM_ERR;
    }
    snprintf(mixer_name, ctl_len, "%s %s", pcm_dev_name, ctl_cmd);
    PAL_DBG(LOG_TAG, "mixer name:%s", mixer_name);
    *ctl = mixer_get_ctl_by_name(mixer, mixer_name);
    if (*ctl == NULL) {
        PAL_ERR(LOG_TAG, "get mixer ctl(%s) failed", mixer_name);
        free(mixer_name);
        mixer_name = NULL;
        return FSM_ERR;
    }
    free(mixer_name);
    mixer_name = NULL;
    return FSM_OK;
}

int fsm_dsp_build_mode_payload(uint32_t tag) {
    uint32_t streamDevicePropId[] = {0x08000010, 1, 0x3};//TODO: port id is 3 or 1? maybe prot d 1 is for stream, port id 2 is for device and port id 3 is for streamdevice
    std::vector <std::pair<int, int>> devicePPCKV;
    std::shared_ptr <ResourceManager> rm = NULL;
    struct agmMetaData streamDeviceMetaData(nullptr, 0);
    //struct fsm_monitor *monitor = fsm_get_monitor();//need detach to monitor if bsg not used
    int device_id = PAL_DEVICE_OUT_SPEAKER;
    std::shared_ptr<Device> dev = nullptr;
    struct mixer *virt_mixer;
    Session *Asession = NULL;
    Stream *Astream = NULL;
    std::string BeName;
    struct mixer_ctl *ctl;
    int ret = 0; uint32_t Spk_Mode = 0;

    rm = ResourceManager::getInstance();

    Spk_Mode = fsm_get_spk_mode(rm);

    switch (tag) {
        case FSM_SPK_MODE_ENABLE:
            PAL_DBG(LOG_TAG, "foursemi set SPK Mode %d", Spk_Mode);
            devicePPCKV.pushback(std::make_pair(SPK_MODE, Spk_Mode));
            break;
        default:
            PAL_DBG(LOG_TAG, "foursemi set default mode to music");
            devicePPCKV.pushback(std::make_pair(SPK_MODE, SPK_MODE_MUSIC));
            break;
    }

    if (devicePPCKV.size() > 0) {
        SessionAlsaUtils::getAgmMetaData(devicePPCKV, devicePPCKV,
                    (struct prop_data *)streamDevicePropId, streamDeviceMetaData);
        if (!streamDeviceMetaData.size) {
            PAL_ERR(LOG_TAG, "stream/device metadata is zero");
            ret = -FSM_ERR;
            goto freeMetaData;
        }
    }
    ret = rm->getBackendName(device_id, BeName);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "get backend name failed");
        goto exit;
    }

    rm->getVirtualAudioMixer(&virt_mixer)
    fsm_dsp_get_ctl_by_pcm_info(virt_mixer, monitor->pcm_device_name, FSM_CTL_CONTROL, &ctl);
    mixer_ctl_set_enum_by_string(ctl, BeName->second.data());
    fsm_dsp_get_ctl_by_pcm_info(virt_mixer, monitor->pcm_device_name, FSM_CTL_METADATA, &ctl);
    if (streamDeviceMetaData.size) {
        mixer_ctl_set_array(ctl, (void *)streamDeviceMetaData.buf,
                streamDeviceMetaData.size);
    }

freeMetaData:
    free(streamDeviceMetaData.buf);
    streamDeviceMetaData.buf = nullptr;
    if (streamDeviceMetaData.buf)
        free(streamDeviceMetaData.buf);

    return ret;
}
#endif

/*
 * param: direction
 *        0 means send
 *        1 means get
**/
struct mixer_ctl* fsm_dsp_get_miid_and_ctl(struct fsalgo_platform_info *info, int direction) {
    int ret = 0;
    std::string BeName;
    struct mixer_ctl *ctl = NULL;
    std::shared_ptr <ResourceManager> rm = NULL;
    int device_id = PAL_DEVICE_OUT_SPEAKER;
    std::shared_ptr<Device> dev = nullptr;
    struct pal_device deviceAttr;
    std::vector<Stream*> Sactive;
    Session *Asession = NULL;
    Stream *Astream = NULL;

    if (info == NULL) {
        PAL_ERR(LOG_TAG, "ptr is null");
        goto exit;
    }
    rm = ResourceManager::getInstance();
    ret = rm->getBackendName(device_id, BeName);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "get backend name failed");
        goto exit;
    }

    deviceAttr.id = (pal_device_id_t)device_id;
    dev = Device::getInstance(&deviceAttr, rm);
    if (dev) {
        ret = rm->getActiveStream_l(Sactive, dev);
        if ((0 != ret) || (Sactive.size() == 0)) {
            PAL_ERR(LOG_TAG, " no active stream available");
            goto exit;
        }
        Astream = static_cast<Stream *>(Sactive[0]);
        Astream->getAssociatedSession(&Asession);
        ret = Asession->getMIID(BeName.c_str(), MODULE_SP, &info->miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info for %x, ret = %d", MODULE_SP, ret);
            goto exit;
        }
        PAL_DBG(LOG_TAG, "miid[0x%x]", info->miid);

        if (direction)
            ctl = Asession->getFEMixerCtl(FSM_CTL_GET_PARAM, &info->fe_pcm);
        else
            ctl = Asession->getFEMixerCtl(FSM_CTL_SET_PARAM, &info->fe_pcm);

        if (info->fe_pcm < 0)
            PAL_ERR(LOG_TAG, "invalid pcm dev");
        info->pcm_device_name = rm->getDeviceNameFromID(info->fe_pcm);
        if (!info->pcm_device_name)
            PAL_ERR(LOG_TAG, "get pcm device name failed");
        //else
            PAL_DBG(LOG_TAG, "pcm dev name:%s", info->pcm_device_name);
    }else {
        PAL_ERR(LOG_TAG, "get device instance falied");
        goto exit;
    }

exit:
    return ctl;
}

int fsm_send_payload_to_dsp(char *data, unsigned int data_size, uint32_t param_id) {
    int ret = 0;
    uint8_t *payload = NULL;
    uint32_t payload_size = 0;
    uint32_t pad_bytes = 0;
    struct mixer_ctl *ctl = NULL;
    struct fs_module_param_data_t *header = NULL;
    struct fsalgo_platform_info info;

    if (data == NULL || data_size == 0 || param_id == 0) {
        PAL_ERR(LOG_TAG, "invalid param");
        return FSM_ERR;
    }
    ctl = fsm_dsp_get_miid_and_ctl(&info, 0);
    if (ctl == NULL) {
        PAL_ERR(LOG_TAG, "get miid failed");
        return FSM_ERR;
    }

    payload_size = sizeof(struct fs_module_param_data_t) + FS_ALIGN_4BYTE(data_size);
    pad_bytes = FS_PADDING_ALIGN_8BYTE(payload_size);
    payload = (uint8_t *)calloc(1, payload_size + pad_bytes);
    if (payload == NULL) {
        PAL_ERR(LOG_TAG, "calloc memory for payload failed");
        return FSM_ERR;
    }

    header = (struct fs_module_param_data_t *)payload;
    header->miid = info.miid;
    header->param_id = param_id;
    header->param_size = payload_size - sizeof(struct fs_module_param_data_t);
    header->error_code = 0;

    memcpy(payload + sizeof(struct fs_module_param_data_t), data, data_size);
    ret = mixer_ctl_set_array(ctl, payload, payload_size + pad_bytes);
    if (ret < 0)
        PAL_ERR(LOG_TAG, "%s send payload failed", __func__);

    free(payload);
    payload = NULL;

    return ret;
}

int fsm_get_payload_from_dsp(char *data, unsigned int data_size, uint32_t param_id) {
    int ret, i;
    uint8_t *payload = NULL;
    uint32_t payload_size = 0;
    struct mixer_ctl *ctl = NULL;
    struct fs_module_param_data_t *header = NULL;
    struct fsalgo_platform_info info;

    if((data == NULL) || (param_id == 0) || (data_size == 0)) {
        PAL_ERR(LOG_TAG, "invalid param, please check");
        return FSM_ERR;
    }

    ctl = fsm_dsp_get_miid_and_ctl(&info, 0);
    if (ctl == NULL) {
        PAL_ERR(LOG_TAG, "get miid failed");
        return FSM_ERR;
    }

    payload_size = FS_ALIGN_8BYTE(sizeof(struct fs_module_param_data_t) + data_size);
    payload = (uint8_t *)calloc(1, payload_size);
    if (payload == NULL) {
        PAL_ERR(LOG_TAG, "calloc memery for paylad failed");
        return FSM_ERR;
    }

    header = (struct fs_module_param_data_t *)payload;
    header->miid = info.miid;
    header->param_id = param_id;
    header->param_size = payload_size - sizeof(struct fs_module_param_data_t);
    header->error_code = 0;

    ret = mixer_ctl_set_array(ctl, payload, payload_size);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "send payload failed");
        goto exit;
    }
    memset(payload, 0, payload_size);
    ret = mixer_ctl_get_array(ctl, payload, payload_size);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "get payload failed");
        goto exit;
    }

    for (i = 0; i < payload_size; i++) {
        PAL_DBG(LOG_TAG, "payload[%d] = 0x%x", i, payload[i]);
    }
    memcpy(data, payload + sizeof(struct fs_module_param_data_t), data_size);
    ret = FSM_OK;

exit:
    free(payload);
    payload = NULL;
    return ret;
}

