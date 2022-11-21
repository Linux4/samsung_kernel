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

#define LOG_TAG "PAL: SessionAlsaCompress"

#include "SessionAlsaCompress.h"
#include "SessionAlsaUtils.h"
#include "Stream.h"
#include "ResourceManager.h"
#include "media_fmt_api.h"
#include "gapless_api.h"
#include <agm/agm_api.h>
#include <sstream>
#include <mutex>
#include <fstream>
#include <agm/agm_api.h>

void SessionAlsaCompress::updateCodecOptions(pal_param_payload *param_payload)
{
    pal_snd_dec_t *pal_snd_dec = nullptr;

    pal_snd_dec = (pal_snd_dec_t *)param_payload->payload;
    PAL_DBG(LOG_TAG, "compress format %x", audio_fmt);
    switch (audio_fmt) {
        case PAL_AUDIO_FMT_MP3:
        case PAL_AUDIO_FMT_COMPRESSED_EXTENDED_RANGE_END:
        case PAL_AUDIO_FMT_AMR_NB:
        case PAL_AUDIO_FMT_AMR_WB:
        case PAL_AUDIO_FMT_AMR_WB_PLUS:
        case PAL_AUDIO_FMT_QCELP:
        case PAL_AUDIO_FMT_EVRC:
        case PAL_AUDIO_FMT_G711:
        break;
        case PAL_AUDIO_FMT_PCM_S16_LE:
        case PAL_AUDIO_FMT_PCM_S8:
        case PAL_AUDIO_FMT_PCM_S24_LE:
        case PAL_AUDIO_FMT_PCM_S24_3LE:
        case PAL_AUDIO_FMT_PCM_S32_LE:
        break;
        case PAL_AUDIO_FMT_COMPRESSED_RANGE_BEGIN:
        case PAL_AUDIO_FMT_COMPRESSED_EXTENDED_RANGE_BEGIN:
        break;
        case PAL_AUDIO_FMT_AAC:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_RAW;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->aac_dec.audio_obj_type;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->aac_dec.pce_bits_size;
            PAL_VERBOSE(LOG_TAG, "format- %x audio_obj_type- %x pce_bits_size- %x",
                        codec.format, codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1]);
        }
        break;
        case PAL_AUDIO_FMT_AAC_ADTS:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_MP4ADTS;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->aac_dec.audio_obj_type;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->aac_dec.pce_bits_size;
            PAL_VERBOSE(LOG_TAG, "format- %x audio_obj_type- %x pce_bits_size- %x",
                        codec.format, codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1]);
        }
        break;
        case PAL_AUDIO_FMT_AAC_ADIF:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_ADIF;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->aac_dec.audio_obj_type;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->aac_dec.pce_bits_size;
            PAL_VERBOSE(LOG_TAG, "format- %x audio_obj_type- %x pce_bits_size- %x",
                        codec.format, codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1]);
        }
        break;
        case PAL_AUDIO_FMT_AAC_LATM:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_MP4LATM;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->aac_dec.audio_obj_type;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->aac_dec.pce_bits_size;
            PAL_VERBOSE(LOG_TAG, "format- %x audio_obj_type- %x pce_bits_size- %x",
                        codec.format, codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1]);
        }
        break;
        case PAL_AUDIO_FMT_WMA_STD:
        {
            codec.format = pal_snd_dec->wma_dec.fmt_tag;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->wma_dec.avg_bit_rate/8;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->wma_dec.super_block_align;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->wma_dec.bits_per_sample;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->wma_dec.channelmask;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->wma_dec.encodeopt;
            codec.options.generic.reserved[5] =
                                    pal_snd_dec->wma_dec.encodeopt1;
            codec.options.generic.reserved[6] =
                                    pal_snd_dec->wma_dec.encodeopt2;
            PAL_VERBOSE(LOG_TAG, "format- %x super_block_align- %x bits_per_sample- %x"
                        ", channelmask - %x \n", codec.format,
                        codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3]);
            PAL_VERBOSE(LOG_TAG, "encodeopt - %x, encodeopt1 - %x, encodeopt2 - %x"
                        ", avg_byte_rate - %x \n", codec.options.generic.reserved[4],
                        codec.options.generic.reserved[5], codec.options.generic.reserved[6],
                        codec.options.generic.reserved[0]);
        }
        break;
        case PAL_AUDIO_FMT_WMA_PRO:
        {
            codec.format = pal_snd_dec->wma_dec.fmt_tag;
            if (pal_snd_dec->wma_dec.fmt_tag == 0x161)
                codec.profile = SND_AUDIOPROFILE_WMA9;
            else if (pal_snd_dec->wma_dec.fmt_tag == 0x162)
                codec.profile = PAL_SND_PROFILE_WMA9_PRO;
            else if (pal_snd_dec->wma_dec.fmt_tag == 0x163)
                codec.profile = PAL_SND_PROFILE_WMA9_LOSSLESS;
            else if (pal_snd_dec->wma_dec.fmt_tag == 0x166)
                codec.profile = SND_AUDIOPROFILE_WMA10;
            else if (pal_snd_dec->wma_dec.fmt_tag == 0x167)
                codec.profile = PAL_SND_PROFILE_WMA10_LOSSLESS;

            codec.options.generic.reserved[0] =
                                    pal_snd_dec->wma_dec.avg_bit_rate/8;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->wma_dec.super_block_align;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->wma_dec.bits_per_sample;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->wma_dec.channelmask;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->wma_dec.encodeopt;
            codec.options.generic.reserved[5] =
                                    pal_snd_dec->wma_dec.encodeopt1;
            codec.options.generic.reserved[6] =
                                    pal_snd_dec->wma_dec.encodeopt2;
            PAL_VERBOSE(LOG_TAG, "format- %x super_block_align- %x"
                        "bits_per_sample- %x channelmask- %x\n",
                        codec.format, codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3]);
            PAL_VERBOSE(LOG_TAG, "encodeopt- %x encodeopt1- %x"
                        "encodeopt2- %x avg_byte_rate- %x\n",
                        codec.options.generic.reserved[4],
                        codec.options.generic.reserved[5],
                        codec.options.generic.reserved[6],
                        codec.options.generic.reserved[0]);
        }
        break;
        case PAL_AUDIO_FMT_ALAC:
        {
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->alac_dec.frame_length;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->alac_dec.compatible_version;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->alac_dec.bit_depth;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->alac_dec.pb;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->alac_dec.mb;
            codec.options.generic.reserved[5] =
                                    pal_snd_dec->alac_dec.kb;
            codec.options.generic.reserved[6] =
                                    pal_snd_dec->alac_dec.max_run;
            codec.options.generic.reserved[7] =
                                   pal_snd_dec->alac_dec.max_frame_bytes;
            codec.options.generic.reserved[8] =
                                   pal_snd_dec->alac_dec.avg_bit_rate;
            codec.options.generic.reserved[9] =
                                   pal_snd_dec->alac_dec.channel_layout_tag;
            PAL_VERBOSE(LOG_TAG, "frame_length- %x compatible_version- %x"
                        "bit_depth- %x pb- %x mb- %x kb- %x",
                        codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3],
                        codec.options.generic.reserved[4],
                        codec.options.generic.reserved[5]);
            PAL_VERBOSE(LOG_TAG, "max_run- %x max_frame_bytes- %x avg_bit_rate- %x"
                        "channel_layout_tag- %x",
                        codec.options.generic.reserved[6],
                        codec.options.generic.reserved[7],
                        codec.options.generic.reserved[8],
                        codec.options.generic.reserved[9]);
        }
        break;
        case PAL_AUDIO_FMT_APE:
        {
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->ape_dec.bits_per_sample;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->ape_dec.compatible_version;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->ape_dec.compression_level;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->ape_dec.format_flags;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->ape_dec.blocks_per_frame;
            codec.options.generic.reserved[5] =
                                    pal_snd_dec->ape_dec.final_frame_blocks;
            codec.options.generic.reserved[6] =
                                    pal_snd_dec->ape_dec.total_frames;
            codec.options.generic.reserved[7] =
                                    pal_snd_dec->ape_dec.seek_table_present;
            PAL_VERBOSE(LOG_TAG, "compatible_version- %x compression_level- %x "
                        "format_flags- %x blocks_per_frame- %x "
                        "final_frame_blocks - %x",
                        codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3],
                        codec.options.generic.reserved[4],
                        codec.options.generic.reserved[5]);
            PAL_VERBOSE(LOG_TAG, "total_frames- %x bits_per_sample- %x"
                        " seek_table_present - %x",
                        codec.options.generic.reserved[6],
                        codec.options.generic.reserved[0],
                        codec.options.generic.reserved[7]);
        }
        break;
        case PAL_AUDIO_FMT_FLAC:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_FLAC;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->flac_dec.sample_size;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->flac_dec.min_blk_size;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->flac_dec.max_blk_size;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->flac_dec.min_frame_size;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->flac_dec.max_frame_size;
            PAL_VERBOSE(LOG_TAG, "sample_size- %x min_blk_size- %x "
                        "max_blk_size- %x min_frame_size- %x "
                        "max_frame_size- %x",
                        codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3],
                        codec.options.generic.reserved[4]);
        }
        break;
        case PAL_AUDIO_FMT_FLAC_OGG:
        {
            codec.format = SND_AUDIOSTREAMFORMAT_FLAC_OGG;
            codec.options.generic.reserved[0] =
                                    pal_snd_dec->flac_dec.sample_size;
            codec.options.generic.reserved[1] =
                                    pal_snd_dec->flac_dec.min_blk_size;
            codec.options.generic.reserved[2] =
                                    pal_snd_dec->flac_dec.max_blk_size;
            codec.options.generic.reserved[3] =
                                    pal_snd_dec->flac_dec.min_frame_size;
            codec.options.generic.reserved[4] =
                                    pal_snd_dec->flac_dec.max_frame_size;
            PAL_VERBOSE(LOG_TAG, "sample_size- %x min_blk_size- %x "
                        "max_blk_size- %x min_frame_size- %x "
                        "max_frame_size - %x",
                        codec.options.generic.reserved[0],
                        codec.options.generic.reserved[1],
                        codec.options.generic.reserved[2],
                        codec.options.generic.reserved[3],
                        codec.options.generic.reserved[4]);
        }
        break;
        case PAL_AUDIO_FMT_VORBIS:
            codec.format = pal_snd_dec->vorbis_dec.bit_stream_fmt;
        break;
        default:
            PAL_ERR(LOG_TAG, "Entered default, format %x", audio_fmt);
        break;
    }
}

void SessionAlsaCompress::getSndCodecParam(struct snd_codec &codec, struct pal_stream_attributes &sAttr)
{
    struct pal_media_config *config = &sAttr.out_media_config;

    codec.id = getSndCodecId(config->aud_fmt_id);
    codec.ch_in = config->ch_info.channels;
    codec.ch_out = codec.ch_in;
    codec.sample_rate = config->sample_rate;
    codec.bit_rate = config->bit_width;
}

int SessionAlsaCompress::getSndCodecId(pal_audio_fmt_t fmt)
{
    int id = -1;

    switch (fmt) {
        case PAL_AUDIO_FMT_MP3:
            id = SND_AUDIOCODEC_MP3;
            break;
        case PAL_AUDIO_FMT_AMR_NB:
        case PAL_AUDIO_FMT_AMR_WB:
        case PAL_AUDIO_FMT_AMR_WB_PLUS:
        case PAL_AUDIO_FMT_QCELP:
        case PAL_AUDIO_FMT_EVRC:
        case PAL_AUDIO_FMT_G711:
             id = -1;
            break;
        case PAL_AUDIO_FMT_COMPRESSED_RANGE_BEGIN:
        case PAL_AUDIO_FMT_COMPRESSED_EXTENDED_RANGE_BEGIN:
        case PAL_AUDIO_FMT_COMPRESSED_EXTENDED_RANGE_END:
            id = -1;
            break;
        case PAL_AUDIO_FMT_AAC:
        case PAL_AUDIO_FMT_AAC_ADTS:
        case PAL_AUDIO_FMT_AAC_ADIF:
        case PAL_AUDIO_FMT_AAC_LATM:
            id = SND_AUDIOCODEC_AAC;
            break;
        case PAL_AUDIO_FMT_WMA_STD:
            id = SND_AUDIOCODEC_WMA;
            break;
        case PAL_AUDIO_FMT_PCM_S8:
        case PAL_AUDIO_FMT_PCM_S16_LE:
        case PAL_AUDIO_FMT_PCM_S24_3LE:
        case PAL_AUDIO_FMT_PCM_S24_LE:
        case PAL_AUDIO_FMT_PCM_S32_LE:
            id = SND_AUDIOCODEC_PCM;
            break;
        case PAL_AUDIO_FMT_ALAC:
            id = SND_AUDIOCODEC_ALAC;
            break;
        case PAL_AUDIO_FMT_APE:
            id = SND_AUDIOCODEC_APE;
            break;
        case PAL_AUDIO_FMT_WMA_PRO:
            id = SND_AUDIOCODEC_WMA;
            break;
        case PAL_AUDIO_FMT_FLAC:
        case PAL_AUDIO_FMT_FLAC_OGG:
            id = SND_AUDIOCODEC_FLAC;
            break;
        case PAL_AUDIO_FMT_VORBIS:
            id = SND_AUDIOCODEC_VORBIS;
            break;
        default:
            PAL_ERR(LOG_TAG, "Entered default format %x", fmt);
            break;
    }

    return id;
}

bool SessionAlsaCompress::isGaplessFormat(pal_audio_fmt_t fmt)
{
    bool isSupported = false;

    /* If platform doesn't support Gapless,
     * then return false for all formats.
     */
    if (!(rm->isGaplessEnabled)) {
        return isSupported;
    }
    switch (fmt) {
        case PAL_AUDIO_FMT_DEFAULT_COMPRESSED:
        case PAL_AUDIO_FMT_AAC:
        case PAL_AUDIO_FMT_AAC_ADTS:
        case PAL_AUDIO_FMT_AAC_ADIF:
        case PAL_AUDIO_FMT_AAC_LATM:
        case PAL_AUDIO_FMT_VORBIS:
        case PAL_AUDIO_FMT_FLAC:
        case PAL_AUDIO_FMT_WMA_PRO:
        case PAL_AUDIO_FMT_APE:
        case PAL_AUDIO_FMT_WMA_STD:
            isSupported = true;
            break;
        case PAL_AUDIO_FMT_PCM_S8:
        case PAL_AUDIO_FMT_PCM_S16_LE:
        case PAL_AUDIO_FMT_PCM_S24_3LE:
        case PAL_AUDIO_FMT_PCM_S24_LE:
        case PAL_AUDIO_FMT_PCM_S32_LE:
            break;
        case PAL_AUDIO_FMT_ALAC:
            break;
        case PAL_AUDIO_FMT_FLAC_OGG:
            break;
        default:
            break;
    }
    PAL_DBG(LOG_TAG, "format %x, gapless supported %d", audio_fmt,
                      isSupported);
    return isSupported;
}

bool SessionAlsaCompress::isCodecConfigNeeded(pal_audio_fmt_t audio_fmt)
{
    bool ret = false;
    switch (audio_fmt) {
        case PAL_AUDIO_FMT_VORBIS:
        case PAL_AUDIO_FMT_FLAC:
        case PAL_AUDIO_FMT_WMA_PRO:
        case PAL_AUDIO_FMT_APE:
        case PAL_AUDIO_FMT_WMA_STD:
            ret = true;
            break;
        case PAL_AUDIO_FMT_DEFAULT_COMPRESSED:
        case PAL_AUDIO_FMT_AAC:
        case PAL_AUDIO_FMT_AAC_ADTS:
        case PAL_AUDIO_FMT_AAC_ADIF:
        case PAL_AUDIO_FMT_AAC_LATM:
        case PAL_AUDIO_FMT_PCM_S8:
        case PAL_AUDIO_FMT_PCM_S16_LE:
        case PAL_AUDIO_FMT_PCM_S24_3LE:
        case PAL_AUDIO_FMT_PCM_S24_LE:
        case PAL_AUDIO_FMT_PCM_S32_LE:
        case PAL_AUDIO_FMT_ALAC:
        case PAL_AUDIO_FMT_FLAC_OGG:
            break;
        default:
            break;
    }
    PAL_DBG(LOG_TAG, "format %x, need to send codec config %d", audio_fmt,
                      ret);
    return ret;
}

int SessionAlsaCompress::setCustomFormatParam(pal_audio_fmt_t audio_fmt)
{
    int32_t status = 0;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    uint32_t miid;
    struct pal_stream_attributes sAttr;
    struct media_format_t *media_fmt_hdr = nullptr;
    struct agm_buff buffer = {0, 0, 0, NULL, 0, NULL, {0, 0, 0}};

    if (audio_fmt == PAL_AUDIO_FMT_VORBIS) {
        payload_media_fmt_vorbis_t* media_fmt_vorbis = NULL;
        // set config for vorbis, as it cannot be upstreamed.
        status = SessionAlsaUtils::getModuleInstanceId(mixer,
                    compressDevIds.at(0), rxAifBackEnds[0].second.data(),
                    STREAM_INPUT_MEDIA_FORMAT, &miid);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "getModuleInstanceId failed");
            return status;
        }
        media_fmt_hdr = (struct media_format_t *)
                            malloc(sizeof(struct media_format_t)
                                + sizeof(struct pal_snd_dec_vorbis));
        if (!media_fmt_hdr) {
            PAL_ERR(LOG_TAG, "failed to allocate memory");
            return -ENOMEM;
        }
        media_fmt_hdr->data_format = DATA_FORMAT_RAW_COMPRESSED ;
        media_fmt_hdr->fmt_id = MEDIA_FMT_ID_VORBIS;
        media_fmt_hdr->payload_size = sizeof(struct pal_snd_dec_vorbis);
        media_fmt_vorbis = (payload_media_fmt_vorbis_t*)(((uint8_t*)media_fmt_hdr) +
            sizeof(struct media_format_t));

        ar_mem_cpy(media_fmt_vorbis,
                            sizeof(struct pal_snd_dec_vorbis),
                            &codec.format,
                            sizeof(struct pal_snd_dec_vorbis));
        if (sendNextTrackParams) {
            PAL_DBG(LOG_TAG, "sending next track param on datapath");
            buffer.timestamp = 0x0;
            buffer.flags = AGM_BUFF_FLAG_MEDIA_FORMAT;
            buffer.size = sizeof(struct media_format_t) +
                          sizeof(struct pal_snd_dec_vorbis);
            buffer.addr = (uint8_t *)media_fmt_hdr;
            payload = (uint8_t *)&buffer;
            payloadSize = sizeof(struct agm_buff);
            status = SessionAlsaUtils::mixerWriteDatapathParams(mixer,
                        compressDevIds.at(0), payload, payloadSize);
            free(media_fmt_hdr);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"mixerWriteWithMetadata failed %d", status);
                return status;
            }
            sendNextTrackParams = false;
        } else {
            status = builder->payloadCustomParam(&payload, &payloadSize,
                                        (uint32_t *)media_fmt_hdr,
                                        sizeof(struct media_format_t) +
                                        sizeof(struct pal_snd_dec_vorbis),
                                        miid, PARAM_ID_MEDIA_FORMAT);
            free(media_fmt_hdr);
            if (status) {
                PAL_ERR(LOG_TAG,"payloadCustomParam failed status = %d", status);
                return status;
            }
            status = SessionAlsaUtils::setMixerParameter(mixer,
                            compressDevIds.at(0), payload, payloadSize);
            freeCustomPayload(&payload, &payloadSize);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"setMixerParameter failed");
                return status;
            }
        }
    }
    return status;
}

void SessionAlsaCompress::offloadThreadLoop(SessionAlsaCompress* compressObj)
{
    std::shared_ptr<offload_msg> msg;
    uint32_t event_id = 0;
    int ret = 0;
    bool is_drain_called = false;
    std::unique_lock<std::mutex> lock(compressObj->cv_mutex_);

    while (1) {
        if (compressObj->msg_queue_.empty())
            compressObj->cv_.wait(lock);  /* wait for incoming requests */

        if (!compressObj->msg_queue_.empty()) {
            msg = compressObj->msg_queue_.front();
            compressObj->msg_queue_.pop();
            lock.unlock();

            if (msg && msg->cmd == OFFLOAD_CMD_EXIT)
                break; // exit the thread

            if (msg && msg->cmd == OFFLOAD_CMD_WAIT_FOR_BUFFER) {
                if (compressObj->rm->cardState == CARD_STATUS_ONLINE) {
                    PAL_VERBOSE(LOG_TAG, "calling compress_wait");
                    ret = compress_wait(compressObj->compress, -1);
                    PAL_VERBOSE(LOG_TAG, "out of compress_wait, ret %d", ret);
                    event_id = PAL_STREAM_CBK_EVENT_WRITE_READY;
                }
            } else if (msg && msg->cmd == OFFLOAD_CMD_DRAIN) {
                if (!is_drain_called && compressObj->playback_started) {
                    PAL_INFO(LOG_TAG, "calling compress_drain");
                    if (compressObj->rm->cardState == CARD_STATUS_ONLINE) {
                         ret = compress_drain(compressObj->compress);
                         PAL_INFO(LOG_TAG, "out of compress_drain, ret %d", ret);
                    }
                }
                if (ret == -ENETRESET) {
                    PAL_ERR(LOG_TAG, "Block drain ready event during SSR");
                    lock.lock();
                    continue;
                }
                is_drain_called = false;
                event_id = PAL_STREAM_CBK_EVENT_DRAIN_READY;
            } else if (msg && msg->cmd == OFFLOAD_CMD_PARTIAL_DRAIN) {
                if (compressObj->playback_started) {
                    if (compressObj->rm->cardState == CARD_STATUS_ONLINE) {
                        if (compressObj->isGaplessFmt) {
                            PAL_DBG(LOG_TAG, "calling partial compress_drain");
                            ret = compress_next_track(compressObj->compress);
                            PAL_INFO(LOG_TAG, "out of compress next track, ret %d", ret);
                            if (ret == 0) {
                                ret = compress_partial_drain(compressObj->compress);
                                PAL_INFO(LOG_TAG, "out of partial compress_drain, ret %d", ret);
                            }
                            event_id = PAL_STREAM_CBK_EVENT_PARTIAL_DRAIN_READY;
                        } else {
                            PAL_DBG(LOG_TAG, "calling compress_drain");
                            ret = compress_drain(compressObj->compress);
                            PAL_INFO(LOG_TAG, "out of compress_drain, ret %d", ret);
                            is_drain_called = true;
                            event_id = PAL_STREAM_CBK_EVENT_DRAIN_READY;
                        }
                    }
                } else {
                    PAL_ERR(LOG_TAG, "Playback not started yet");
                    event_id = PAL_STREAM_CBK_EVENT_DRAIN_READY;
                    is_drain_called = true;
                }
                if (ret == -ENETRESET) {
                    PAL_ERR(LOG_TAG, "Block drain ready event during SSR");
                    lock.lock();
                    continue;
                }
            }  else if (msg && msg->cmd == OFFLOAD_CMD_ERROR) {
                PAL_ERR(LOG_TAG, "Sending error to PAL client");
                event_id = PAL_STREAM_CBK_EVENT_ERROR;
            }
            if (compressObj->sessionCb)
                compressObj->sessionCb(compressObj->cbCookie, event_id, NULL, 0);

            lock.lock();
        }

    }
    PAL_DBG(LOG_TAG, "exit offloadThreadLoop");
}

SessionAlsaCompress::SessionAlsaCompress(std::shared_ptr<ResourceManager> Rm)
{
    rm = Rm;
    builder = new PayloadBuilder();

    /** set default snd codec params */
    codec.id = getSndCodecId(PAL_AUDIO_FMT_PCM_S16_LE);
    codec.ch_in = 2;
    codec.ch_out = codec.ch_in;
    codec.sample_rate = 48000;
    codec.bit_rate = 16;
    customPayload = NULL;
    customPayloadSize = 0;
    compress = NULL;
    sessionCb = NULL;
    this->cbCookie = 0;
    playback_started = false;
    playback_paused = false;
    streamHandle = NULL;
}

SessionAlsaCompress::~SessionAlsaCompress()
{
    delete builder;
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    compressDevIds.clear();
#endif
}

int SessionAlsaCompress::open(Stream * s)
{
    int status = -EINVAL;
    struct pal_stream_attributes sAttr;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    std::vector<std::pair<int32_t, std::string>> emptyBackEnds;

    PAL_DBG(LOG_TAG,"Enter");
    status = s->getStreamAttributes(&sAttr);
    streamHandle = s;
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getStreamAttributes Failed \n");
        goto exit;
    }

    ioMode = sAttr.flags & PAL_STREAM_FLAG_NON_BLOCKING_MASK;
    if (!ioMode) {
        PAL_ERR(LOG_TAG, "IO mode 0x%x not supported", ioMode);
        status = -EINVAL;
        goto exit;
    }
    status = s->getAssociatedDevices(associatedDevices);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
        goto exit;
    }

    compressDevIds = rm->allocateFrontEndIds(sAttr, 0);
    if (compressDevIds.size() == 0) {
        PAL_ERR(LOG_TAG, "no more FE vailable");
        return -EINVAL;
    }
    for (int i = 0; i < compressDevIds.size(); i++) {
        //compressDevIds[i] = 5;
        PAL_DBG(LOG_TAG, "devid size %zu, compressDevIds[%d] %d", compressDevIds.size(), i, compressDevIds[i]);
    }
    rm->getBackEndNames(associatedDevices, rxAifBackEnds, emptyBackEnds);
    status = rm->getVirtualAudioMixer(&mixer);
    if (status) {
        PAL_ERR(LOG_TAG,"mixer error");
        goto exit;
    }
    status = SessionAlsaUtils::open(s, rm, compressDevIds, rxAifBackEnds);
    if (status) {
        PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
        rm->freeFrontEndIds(compressDevIds, sAttr, 0);
    }
    audio_fmt = sAttr.out_media_config.aud_fmt_id;
    isGaplessFmt = isGaplessFormat(audio_fmt);

    // Register for  mixer event callback
    status = rm->registerMixerEventCallback(compressDevIds, sessionCb, cbCookie,
                    true);
    if (status != 0) {
        // Not a fatal error. Only pop noise will come for Soft pause use case
        PAL_ERR(LOG_TAG, "Failed to register callback to rm");
        status = 0;
        isPauseRegistrationDone = false;
    }
    else
        isPauseRegistrationDone = true;

exit:
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::disconnectSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToDisconnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToDisconnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToDisconnect;
    int32_t status = 0;

    deviceList.push_back(deviceToDisconnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToDisconnect,
            txAifBackEndsToDisconnect);
    deviceToDisconnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToDisconnect.empty()) {
        int cnt = 0;

        status = SessionAlsaUtils::disconnectSessionDevice(streamHandle, streamType, rm,
            dAttr, compressDevIds, rxAifBackEndsToDisconnect);

        for (const auto &elem : rxAifBackEnds) {
            cnt++;
            for (const auto &disConnectElem : rxAifBackEndsToDisconnect) {
                if (std::get<0>(elem) == std::get<0>(disConnectElem)) {
                    rxAifBackEnds.erase(rxAifBackEnds.begin() + cnt - 1, rxAifBackEnds.begin() + cnt);
                    cnt--;
                    break;
                }
            }
        }
    }

    if (!txAifBackEndsToDisconnect.empty()) {
        int cnt = 0;

        status = SessionAlsaUtils::disconnectSessionDevice(streamHandle, streamType, rm,
            dAttr, compressDevIds, txAifBackEndsToDisconnect);

        for (const auto &elem : txAifBackEnds) {
            cnt++;
            for (const auto &disConnectElem : txAifBackEndsToDisconnect) {
                if (std::get<0>(elem) == std::get<0>(disConnectElem)) {
                    txAifBackEnds.erase(txAifBackEnds.begin() + cnt - 1, txAifBackEnds.begin() + cnt);
                    cnt--;
                    break;
                }
            }
        }
    }

    return status;
}

int SessionAlsaCompress::setupSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToConnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToConnect;
    int32_t status = 0;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToConnect,
            txAifBackEndsToConnect);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToConnect.empty())
        status = SessionAlsaUtils::setupSessionDevice(streamHandle, streamType, rm,
            dAttr, compressDevIds, rxAifBackEndsToConnect);

    if (!txAifBackEndsToConnect.empty())
        status = SessionAlsaUtils::setupSessionDevice(streamHandle, streamType, rm,
            dAttr, compressDevIds, txAifBackEndsToConnect);

    return status;
}

int SessionAlsaCompress::connectSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToConnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToConnect;
    int32_t status = 0;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToConnect,
            txAifBackEndsToConnect);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToConnect.empty()) {
        status = SessionAlsaUtils::connectSessionDevice(this, streamHandle, streamType, rm,
            dAttr, compressDevIds, rxAifBackEndsToConnect);
        for (const auto &elem : rxAifBackEndsToConnect)
            rxAifBackEnds.push_back(elem);
    }

    if (!txAifBackEndsToConnect.empty()) {
        status = SessionAlsaUtils::connectSessionDevice(this, streamHandle, streamType, rm,
            dAttr, compressDevIds, txAifBackEndsToConnect);
        for (const auto &elem : txAifBackEndsToConnect)
            txAifBackEnds.push_back(elem);

    }

    return status;
}

int SessionAlsaCompress::prepare(Stream * s __unused)
{
   return 0;
}

int SessionAlsaCompress::setTKV(Stream * s __unused, configType type, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    uint32_t tagsent;
    struct agm_tag_config* tagConfig = nullptr;
    const char *setParamTagControl = "setParamTag";
    const char *stream = "COMPRESS";
    struct mixer_ctl *ctl;
    std::ostringstream tagCntrlName;
    int tkv_size = 0;

    switch (type) {
        case MODULE:
        {
            pal_key_vector_t *pal_kvpair = (pal_key_vector_t *)effectPayload->payload;
            uint32_t num_tkvs = pal_kvpair->num_tkvs;
            for (uint32_t i = 0; i < num_tkvs; i++) {
                tkv.push_back(std::make_pair(pal_kvpair->kvp[i].key, pal_kvpair->kvp[i].value));
            }

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));

            if (!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }

            tagsent = effectPayload->tag;
            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }
            tagCntrlName<<stream<<compressDevIds.at(0)<<" "<<setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                status = -ENOENT;
                goto exit;
            }
            PAL_VERBOSE(LOG_TAG, "mixer control: %s\n", tagCntrlName.str().data());

            tkv_size = tkv.size()*sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            tkv.clear();

            break;
        }
        default:
            PAL_ERR(LOG_TAG,"invalid type ");
            status = -EINVAL;
            goto exit;
    }

exit:
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    if (tagConfig) {
        free(tagConfig);
        tagConfig = nullptr;
    }

    return status;
}

int SessionAlsaCompress::setConfig(Stream * s, configType type, uint32_t tag1,
        uint32_t tag2, uint32_t tag3)
{
    int status = 0;
    uint32_t tagsent = 0;
    struct agm_tag_config* tagConfig = nullptr;
    std::ostringstream tagCntrlName;
    char const *stream = "COMPRESS";
    const char *setParamTagControl = "setParamTag";
    struct mixer_ctl *ctl = nullptr;
    uint32_t tkv_size = 0;

    PAL_DBG(LOG_TAG, "Enter");
    switch (type) {
        case MODULE:
            tkv.clear();
            if (tag1)
                builder->populateTagKeyVector(s, tkv, tag1, &tagsent);
            if (tag2)
                builder->populateTagKeyVector(s, tkv, tag2, &tagsent);
            if (tag3)
                builder->populateTagKeyVector(s, tkv, tag3, &tagsent);

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }
            tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));
            if (!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }
            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }
            tagCntrlName << stream << compressDevIds.at(0) << " " << setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                status = -ENOENT;
                goto exit;
            }

            tkv_size = tkv.size() * sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            tkv.clear();
            break;
        default:
            status = 0;
            break;
    }

exit:
    if(tagConfig)
        free(tagConfig);
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;
}

int SessionAlsaCompress::setConfig(Stream * s, configType type, int tag)
{
    int status = 0;
    uint32_t tagsent;
    struct agm_tag_config* tagConfig;
    const char *setParamTagControl = "setParamTag";
    const char *stream = "COMPRESS";
    const char *setCalibrationControl = "setCalibration";
    struct mixer_ctl *ctl;
    struct agm_cal_config *calConfig;
    std::ostringstream tagCntrlName;
    std::ostringstream calCntrlName;
    int tkv_size = 0;
    int ckv_size = 0;

    PAL_DBG(LOG_TAG, "Enter");
    switch (type) {
        case MODULE:
            status = builder->populateTagKeyVector(s, tkv, tag, &tagsent);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Failed to set the tag configuration\n");
                goto exit;
            }

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));

            if (!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }

            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                free(tagConfig);
#endif
                goto exit;
            }
            //TODO: how to get the id '5'
            tagCntrlName<<stream<<compressDevIds.at(0)<<" "<<setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                if (tagConfig)
                    free(tagConfig);
#endif
                return -ENOENT;
            }
            PAL_VERBOSE(LOG_TAG, "mixer control: %s\n", tagCntrlName.str().data());

            tkv_size = tkv.size()*sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
            }
            ctl = NULL;
            if (tagConfig)
                free(tagConfig);
            tkv.clear();
            break;
            //todo calibration
        case CALIBRATION:
            status = builder->populateCalKeyVector(s, ckv, tag);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Failed to set the calibration data\n");
                goto exit;
            }
            if (ckv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            calConfig = (struct agm_cal_config*)malloc (sizeof(struct agm_cal_config) +
                            (ckv.size() * sizeof(agm_key_value)));
            if (!calConfig) {
                status = -EINVAL;
                goto exit;
            }
            status = SessionAlsaUtils::getCalMetadata(ckv, calConfig);
            //TODO: how to get the id '0'
            calCntrlName<<stream<<compressDevIds.at(0)<<" "<<setCalibrationControl;
            ctl = mixer_get_ctl_by_name(mixer, calCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", calCntrlName.str().data());
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                if (calConfig)
                    free(calConfig);
#endif
                return -ENOENT;
            }
            PAL_VERBOSE(LOG_TAG, "mixer control: %s\n", calCntrlName.str().data());
            ckv_size = ckv.size()*sizeof(struct agm_key_value);
            //TODO make struct mixer and struct pcm as class private variables.
            status = mixer_ctl_set_array(ctl, calConfig, sizeof(struct agm_cal_config) + ckv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
            }
            ctl = NULL;
            if (calConfig)
                free(calConfig);
            ckv.clear();
            break;
        default:
            PAL_ERR(LOG_TAG,"invalid type ");
            status = -EINVAL;
            break;
    }

exit:
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    return status;
}

int SessionAlsaCompress::configureEarlyEOSDelay(void)
{
    int32_t status = 0;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    uint32_t miid;

    PAL_DBG(LOG_TAG, "Enter");
    status = SessionAlsaUtils::getModuleInstanceId(mixer,
                    compressDevIds.at(0), rxAifBackEnds[0].second.data(),
                    MODULE_GAPLESS, &miid);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "getModuleInstanceId failed");
        return status;
    }
    param_id_gapless_early_eos_delay_t  *early_eos_delay = nullptr;
    early_eos_delay = (struct param_id_gapless_early_eos_delay_t *)
                            malloc(sizeof(struct param_id_gapless_early_eos_delay_t));
    if (!early_eos_delay) {
        PAL_ERR(LOG_TAG, "failed to allocate memory");
        return -ENOMEM;
    }
    early_eos_delay->early_eos_delay_ms = EARLY_EOS_DELAY_MS;

    status = builder->payloadCustomParam(&payload, &payloadSize,
                                        (uint32_t *)early_eos_delay,
                                        sizeof(struct param_id_gapless_early_eos_delay_t),
                                        miid, PARAM_ID_EARLY_EOS_DELAY);
    free(early_eos_delay);
    if (status) {
        PAL_ERR(LOG_TAG,"payloadCustomParam failed status = %d", status);
        return status;
    }
    if (payloadSize) {
        status = updateCustomPayload(payload, payloadSize);
        freeCustomPayload(&payload, &payloadSize);
        if(0 != status) {
            PAL_ERR(LOG_TAG,"%s: updateCustomPayload Failed\n", __func__);
            return status;
        }
    }
    return status;
}

int SessionAlsaCompress::start(Stream * s)
{
    struct compr_config compress_config;
    struct pal_stream_attributes sAttr;
    int32_t status = 0;
    size_t in_buf_size, in_buf_count, out_buf_size, out_buf_count;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct pal_device dAttr;

    PAL_DBG(LOG_TAG,"Enter");
    /** create an offload thread for posting callbacks */
    worker_thread = std::make_unique<std::thread>(offloadThreadLoop, this);

    rm->voteSleepMonitor(s, true);
    s->getStreamAttributes(&sAttr);
    getSndCodecParam(codec, sAttr);
    s->getBufInfo(&in_buf_size,&in_buf_count,&out_buf_size,&out_buf_count);
    compress_config.fragment_size = out_buf_size;
    compress_config.fragments = out_buf_count;
    compress_config.codec = &codec;
    // compress_open
    compress = compress_open(rm->getVirtualSndCard(), compressDevIds.at(0), COMPRESS_IN, &compress_config);
    if (!compress) {
        PAL_ERR(LOG_TAG, "compress open failed");
        status = -EINVAL;
        goto exit;
    }
    if (!is_compress_ready(compress)) {
        PAL_ERR(LOG_TAG, "compress open not ready %s", compress_get_error(compress));
        status = -EINVAL;
        goto exit;
    }
    /** set non blocking mode for writes */
    compress_nonblock(compress, !!ioMode);

    switch (sAttr.direction) {
        case PAL_AUDIO_OUTPUT:
            status = s->getAssociatedDevices(associatedDevices);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
                goto exit;
            }
            rm->getBackEndNames(associatedDevices, rxAifBackEnds, txAifBackEnds);
            if (rxAifBackEnds.empty() && txAifBackEnds.empty()) {
                PAL_ERR(LOG_TAG, "no backend specified for this stream");
                goto exit;

            }

            status = SessionAlsaUtils::getModuleInstanceId(mixer, (compressDevIds.at(0)),
                     rxAifBackEnds[0].second.data(), STREAM_SPR, &spr_miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", STREAM_SPR, status);
                goto exit;;
            }
            setCustomFormatParam(audio_fmt);
            for (int i = 0; i < associatedDevices.size();i++) {
                status = associatedDevices[i]->getDeviceAttributes(&dAttr);
                if(0 != status) {
                    PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
                    goto exit;
                }
                status = configureMFC(rm, sAttr, dAttr, compressDevIds,
                            rxAifBackEnds[i].second.data());
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"configure MFC failed");
                    goto exit;
                }

                if (isGaplessFmt) {
                    status = configureEarlyEOSDelay();
                }

                if (!status && isPauseRegistrationDone) {
                    // Register for callback for Soft Pause
                    size_t payload_size = 0;
                    struct agm_event_reg_cfg event_cfg;
                    payload_size = sizeof(struct agm_event_reg_cfg);
                    memset(&event_cfg, 0, sizeof(event_cfg));
                    event_cfg.event_id = EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE;
                    event_cfg.event_config_payload_size = 0;
                    event_cfg.is_register = 1;
                    status = SessionAlsaUtils::registerMixerEvent(mixer,
                                    compressDevIds.at(0), rxAifBackEnds[0].second.data(),
                                    TAG_PAUSE, (void *)&event_cfg, payload_size);
                    if (status != 0) {
                        PAL_DBG(LOG_TAG,"Unable to register callback for pause\n");
                        status = 0;
                        isPauseRegistrationDone = false;
                    }
                }
            }
            break;
        default:
            break;
    }
    // Setting the volume as no default volume is set now in stream open
    if (setConfig(s, CALIBRATION, TAG_STREAM_VOLUME) != 0) {
            PAL_ERR(LOG_TAG,"Setting volume failed");
    }

exit:
    if (status != 0)
        rm->voteSleepMonitor(s, false);
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::pause(Stream * s __unused)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG,"Enter");

    if (compress && playback_started) {
        status = compress_pause(compress);
        if (status == 0)
            playback_paused = true;
    }
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::resume(Stream * s __unused)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG,"Enter");

    if (compress && playback_paused) {
        status = compress_resume(compress);
        if (status == 0)
            playback_paused = false;
    }
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::stop(Stream * s __unused)
{
    int32_t status = 0;
    size_t payload_size = 0;
    struct agm_event_reg_cfg event_cfg;

    // Deregister for callback for Soft Pause
    if (isPauseRegistrationDone) {
        payload_size = sizeof(struct agm_event_reg_cfg);
        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_id = EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE;
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 0;
        SessionAlsaUtils::registerMixerEvent(mixer, compressDevIds.at(0),
                    rxAifBackEnds[0].second.data(), TAG_PAUSE, (void *)&event_cfg,
                    payload_size);
        isPauseRegistrationDone = false;
    }

    PAL_DBG(LOG_TAG,"Enter");
    if (compress && playback_started) {
        status = compress_stop(compress);
    }
    rm->voteSleepMonitor(s, false);
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::close(Stream * s)
{
    struct pal_stream_attributes sAttr;
    int32_t status = 0;
    std::ostringstream disconnectCtrlName;

    PAL_DBG(LOG_TAG,"Enter");

    s->getStreamAttributes(&sAttr);
    if (!compress) {
        if (compressDevIds.size() != 0)
            rm->freeFrontEndIds(compressDevIds, sAttr, 0);
        if (rm->cardState == CARD_STATUS_OFFLINE) {
            if (sessionCb)
                sessionCb(cbCookie, PAL_STREAM_CBK_EVENT_ERROR, NULL, 0);
        }
        goto exit;
        /** close unstarted session should return normal. */
    }
    disconnectCtrlName << "COMPRESS" << compressDevIds.at(0) << " disconnect";
    disconnectCtrl = mixer_get_ctl_by_name(mixer, disconnectCtrlName.str().data());
    if (!disconnectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", disconnectCtrlName.str().data());
        status = -EINVAL;
        goto exit;
    }
    /** Disconnect FE to BE */
    mixer_ctl_set_enum_by_string(disconnectCtrl, rxAifBackEnds[0].second.data());
    compress_close(compress);
    PAL_DBG(LOG_TAG, "out of compress close");

    rm->freeFrontEndIds(compressDevIds, sAttr, 0);
    freeCustomPayload();

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        std::shared_ptr<offload_msg> msg = std::make_shared<offload_msg>(OFFLOAD_CMD_ERROR);
        std::lock_guard<std::mutex> lock(cv_mutex_);
        msg_queue_.push(msg);
        cv_.notify_all();
    }
    {
        std::shared_ptr<offload_msg> msg = std::make_shared<offload_msg>(OFFLOAD_CMD_EXIT);
        std::lock_guard<std::mutex> lock(cv_mutex_);
        msg_queue_.push(msg);
        cv_.notify_all();
    }

    /* wait for handler to exit */
    worker_thread->join();
    worker_thread.reset(NULL);

    /* empty the pending messages in queue */
    while (!msg_queue_.empty())
        msg_queue_.pop();

    // Deregister for mixer event callback
    status = rm->registerMixerEventCallback(compressDevIds, sessionCb, cbCookie,
                    false);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to deregister callback to rm");
        status = 0;
    }

 exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::read(Stream *s __unused, int tag __unused, struct pal_buffer *buf __unused, int * size __unused)
{
    return 0;
}

int SessionAlsaCompress::fileWrite(Stream *s __unused, int tag __unused, struct pal_buffer *buf, int * size, int flag __unused)
{
    std::fstream fs;
    PAL_DBG(LOG_TAG, "Enter.");

    fs.open ("/data/testcompr.wav", std::fstream::binary | std::fstream::out | std::fstream::app);
    PAL_ERR(LOG_TAG, "file open success");
    char *buff= reinterpret_cast<char *>(buf->buffer);
    fs.write (buff,buf->size);
    PAL_ERR(LOG_TAG, "file write success");
    fs.close();
    PAL_ERR(LOG_TAG, "file close success");
    *size = (int)(buf->size);
    PAL_ERR(LOG_TAG,"Exit. size: %d", *size);
    return 0;
}

int SessionAlsaCompress::write(Stream *s __unused, int tag __unused, struct pal_buffer *buf, int * size, int flag __unused)
{
    int bytes_written = 0;
    int status;
    bool non_blocking = (!!ioMode);
    if (!buf || !(buf->buffer) || !(buf->size)) {
        PAL_VERBOSE(LOG_TAG, "buf: %pK, size: %zu",
                    buf, (buf ? buf->size : 0));
        return -EINVAL;
    }
    if (!compress) {
        PAL_ERR(LOG_TAG, "NULL pointer access,compress is invalid");
        return -EINVAL;
    }

    PAL_DBG(LOG_TAG, "buf->size is %zu buf->buffer is %pK ",
            buf->size, buf->buffer);

    bytes_written = compress_write(compress, buf->buffer, buf->size);

    PAL_VERBOSE(LOG_TAG, "writing buffer (%zu bytes) to compress device returned %d",
             buf->size, bytes_written);

    if (bytes_written >= 0 && bytes_written < (ssize_t)buf->size && non_blocking) {
        PAL_DBG(LOG_TAG, "No space available in compress driver, post msg to cb thread");
        std::shared_ptr<offload_msg> msg = std::make_shared<offload_msg>(OFFLOAD_CMD_WAIT_FOR_BUFFER);
        std::lock_guard<std::mutex> lock(cv_mutex_);
        msg_queue_.push(msg);

        cv_.notify_all();
    }

    if (!playback_started && bytes_written > 0) {
        status = compress_start(compress);
        if (status) {
            PAL_ERR(LOG_TAG, "compress start failed with err %d", status);
            return status;
        }
        playback_started = true;
    }

    if (size)
        *size = bytes_written;
    return 0;
}

int SessionAlsaCompress::readBufferInit(Stream *s __unused, size_t noOfBuf __unused, size_t bufSize __unused, int flag __unused)
{
    return 0;
}
int SessionAlsaCompress::writeBufferInit(Stream *s __unused, size_t noOfBuf __unused, size_t bufSize __unused, int flag __unused)
{
    return 0;
}

struct mixer_ctl* SessionAlsaCompress::getFEMixerCtl(const char *controlName, int *device)
{
    *device = compressDevIds.at(0);
    std::ostringstream CntrlName;
    struct mixer_ctl *ctl;

    CntrlName << "COMPRESS" << compressDevIds.at(0) << " " << controlName;
    ctl = mixer_get_ctl_by_name(mixer, CntrlName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
        return nullptr;
    }

    return ctl;
}

uint32_t SessionAlsaCompress::getMIID(const char *backendName, uint32_t tagId, uint32_t *miid)
{
    int status = 0;
    int device = compressDevIds.at(0);

    status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                   backendName,
                                                   tagId, miid);
    if (0 != status)
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);

    return status;
}

int SessionAlsaCompress::setParameters(Stream *s __unused, int tagId, uint32_t param_id, void *payload)
{
    pal_param_payload *param_payload = (pal_param_payload *)payload;
    int status = 0;
    int device = compressDevIds.at(0);
    uint8_t* alsaParamData = NULL;
    size_t alsaPayloadSize = 0;
    uint32_t miid = 0;
    effect_pal_payload_t *effectPalPayload = nullptr;
    struct compr_gapless_mdata mdata;
    struct pal_compr_gapless_mdata *gaplessMdata = NULL;
    struct pal_stream_attributes sAttr;

    PAL_DBG(LOG_TAG,"Enter");

    switch (param_id) {
        case PAL_PARAM_ID_DEVICE_ROTATION:
        {
            pal_param_device_rotation_t *rotation =
                                     (pal_param_device_rotation_t *)payload;
            status = handleDeviceRotation(s, rotation->rotation_type, device, mixer,
                                          builder, rxAifBackEnds);
        }
        break;
        case PAL_PARAM_ID_UIEFFECT:
        {
            pal_effect_custom_payload_t *customPayload;
            param_payload = (pal_param_payload *)payload;
            effectPalPayload = (effect_pal_payload_t *)(param_payload->payload);
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                           rxAifBackEnds[0].second.data(),
                                                           tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                break;
            } else {
                customPayload = (pal_effect_custom_payload_t *)effectPalPayload->payload;
                status = builder->payloadCustomParam(&alsaParamData, &alsaPayloadSize,
                            customPayload->data,
                            effectPalPayload->payloadSize - sizeof(uint32_t),
                            miid, customPayload->paramId);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "payloadCustomParam failed. status = %d",
                                status);
                    break;
                }
                status = SessionAlsaUtils::setMixerParameter(mixer,
                                                             compressDevIds.at(0),
                                                             alsaParamData,
                                                             alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set param status=%d\n", status);
                freeCustomPayload(&alsaParamData, &alsaPayloadSize);
            }
            break;
        }
        case PAL_PARAM_ID_BT_A2DP_TWS_CONFIG:
        {
            pal_bt_tws_payload *tws_payload = (pal_bt_tws_payload *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            builder->payloadTWSConfig(&alsaParamData, &alsaPayloadSize,
                 miid, tws_payload->isTwsMonoModeOn, tws_payload->codecFormat);
            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set tws config status=%d\n", status);
                freeCustomPayload(&alsaParamData, &alsaPayloadSize);
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_LC3_CONFIG:
        {
            pal_bt_lc3_payload *lc3_payload = (pal_bt_lc3_payload *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            builder->payloadLC3Config(&alsaParamData, &alsaPayloadSize,
                 miid, lc3_payload->isLC3MonoModeOn);
            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set lc3 config status=%d\n", status);
                freeCustomPayload(&alsaParamData, &alsaPayloadSize);
            }
            return 0;
        }
#ifdef SEC_PRODUCT_FEATURE_BLUETOOTH_SUPPORT_A2DP_OFFLOAD
        case PAL_PARAM_ID_BT_A2DP_SUSPENDED:
        {
            pal_param_ss_pack_a2dp_suspend_t *a2dp_suspend_payload = (pal_param_ss_pack_a2dp_suspend_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set a2dp_suspend %d\n", a2dp_suspend_payload->a2dp_suspend);

            builder->payloadSsPackSuspendConfig(&alsaParamData, &alsaPayloadSize,
                miid, a2dp_suspend_payload->a2dp_suspend);

            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set a2dp_suspend status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] alsaParamData;
                alsaPayloadSize = 0;
#else
                free(alsaParamData);
#endif
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_DYN_BITRATE:
        {
            pal_param_bta2dp_dynbitrate_t *dynbitrate_payload = (pal_param_bta2dp_dynbitrate_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set bitrate %d\n", dynbitrate_payload->bitrate);

            builder->payloadSsPackBitrate(&alsaParamData, &alsaPayloadSize,
                 miid, dynbitrate_payload->bitrate);
            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set bitrate status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] alsaParamData;
                alsaPayloadSize = 0;
#else
                free(alsaParamData);
#endif
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_SBM_CONFIG:
        {
            pal_param_bta2dp_sbm_t *sbm_payload = (pal_param_bta2dp_sbm_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set sbm config firstParam=%d, secondParam=%d\n",
                                    sbm_payload->firstParam, sbm_payload->secondParam);

            builder->payloadSsPackSbmConfig(&alsaParamData, &alsaPayloadSize,
                 miid, sbm_payload);
            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set sbm config status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] alsaParamData;
                alsaPayloadSize = 0;
#endif
            }
            return 0;
        }
#endif
        case PAL_PARAM_ID_CODEC_CONFIGURATION:
            PAL_DBG(LOG_TAG, "Compress Codec Configuration");
            updateCodecOptions((pal_param_payload *) payload);
            if (compress && audio_fmt != PAL_AUDIO_FMT_VORBIS) {
                /* For some audio fmt, codec configuration is default like
                 * for mp3, and for some it is hardcoded like for aac, in
                 * these cases, we don't need to send codec params to ADSP
                 * again even if it comes from hal as it will not change.
                 */
                if (isCodecConfigNeeded(audio_fmt)) {
                    PAL_DBG(LOG_TAG, "Setting params for second clip for gapless");
                    status = compress_set_codec_params(compress, &codec);
                } else {
                    PAL_INFO(LOG_TAG, "No need to send params for second clip fmt %x", audio_fmt);
                }
            } else if (compress && (audio_fmt == PAL_AUDIO_FMT_VORBIS)) {
                PAL_DBG(LOG_TAG, "Setting params for second clip for gapless");
                sendNextTrackParams = true;
                status = setCustomFormatParam(audio_fmt);
            }
        break;
        case PAL_PARAM_ID_GAPLESS_MDATA:
        {
            if (!compress) {
                PAL_ERR(LOG_TAG, "Compress is invalid");
                status = -EINVAL;
                goto exit;
            }
            if (isGaplessFmt) {
                gaplessMdata = (pal_compr_gapless_mdata*)param_payload->payload;
                PAL_DBG(LOG_TAG, "Setting gapless metadata %d %d",
                                  gaplessMdata->encoderDelay,
                                  gaplessMdata->encoderPadding);
                mdata.encoder_delay = gaplessMdata->encoderDelay;
                mdata.encoder_padding = gaplessMdata->encoderPadding;
                status = compress_set_gapless_metadata(compress, &mdata);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "set gapless metadata failed");
                    goto exit;
                }
             } else {
                 PAL_ERR(LOG_TAG, "audio fmt %x is not gapless", audio_fmt);
                 status = -EINVAL;
                 goto exit;
             }
        }
        break;
        case PAL_PARAM_ID_VOLUME_USING_SET_PARAM:
        {
            pal_param_payload *param_payload = (pal_param_payload *)payload;
            pal_volume_data *vdata = (struct pal_volume_data *)param_payload->payload;
            status = streamHandle->getStreamAttributes(&sAttr);
            if (sAttr.direction == PAL_AUDIO_OUTPUT) {
                device = compressDevIds.at(0);
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                        rxAifBackEnds[0].second.data(), TAG_STREAM_VOLUME, &miid);
            } else {
                status = 0;
                PAL_INFO(LOG_TAG, "Unsupported stream direction %d(ignore)", sAttr.direction);
                goto exit;
            }
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, dir: %d (%d)", tagId,
                       sAttr.direction, status);
                goto exit;
            }

            builder->payloadVolumeConfig(&alsaParamData, &alsaPayloadSize, miid, vdata);
            if (alsaPayloadSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               alsaParamData, alsaPayloadSize);
                PAL_INFO(LOG_TAG, "mixer set volume config status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] alsaParamData;
                alsaPayloadSize = 0;
#else
                freeCustomPayload(&alsaParamData, &alsaPayloadSize);
#endif
            }
            break;
        }
        default:
            PAL_INFO(LOG_TAG, "Unsupported param id %u", param_id);
        break;
    }
exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaCompress::registerCallBack(session_callback cb, uint64_t cookie)
{
    sessionCb = cb;
    cbCookie = cookie;
    return 0;
}

int SessionAlsaCompress::flush()
{
    int status = 0;

    if (!compress) {
        PAL_ERR(LOG_TAG, "Compress is invalid");
        return -EINVAL;
    }
    if (playback_started) {
        PAL_VERBOSE(LOG_TAG,"Enter flush\n");
        status = compress_stop(compress);
        if (!status) {
            playback_started = false;
        }
    }
    PAL_VERBOSE(LOG_TAG,"playback_started %d status %d\n", playback_started,
            status);
    return status;
}

int SessionAlsaCompress::drain(pal_drain_type_t type)
{
    std::shared_ptr<offload_msg> msg;

    if (!compress) {
       PAL_ERR(LOG_TAG, "compress is invalid");
       return -EINVAL;
    }

    PAL_VERBOSE(LOG_TAG, "drain type = %d", type);

    switch (type) {
    case PAL_DRAIN:
    {
        msg = std::make_shared<offload_msg>(OFFLOAD_CMD_DRAIN);
        std::lock_guard<std::mutex> drain_lock(cv_mutex_);
        msg_queue_.push(msg);
        cv_.notify_all();
    }
    break;

    case PAL_DRAIN_PARTIAL:
    {
        msg = std::make_shared<offload_msg>(OFFLOAD_CMD_PARTIAL_DRAIN);
        std::lock_guard<std::mutex> partial_lock(cv_mutex_);
        msg_queue_.push(msg);
        cv_.notify_all();
    }
    break;

    default:
        PAL_ERR(LOG_TAG, "invalid drain type = %d", type);
        return -EINVAL;
    }

    return 0;
}

int SessionAlsaCompress::getParameters(Stream *s __unused, int tagId __unused, uint32_t param_id __unused, void **payload __unused)
{
    return 0;
}

int SessionAlsaCompress::getTimestamp(struct pal_session_time *stime)
{
    int status = 0;
    status = SessionAlsaUtils::getTimestamp(mixer, compressDevIds, spr_miid, stime);
    if (0 != status) {
       PAL_ERR(LOG_TAG, "getTimestamp failed status = %d", status);
       return status;
    }
    return status;
}

int SessionAlsaCompress::setECRef(Stream *s __unused, std::shared_ptr<Device> rx_dev __unused, bool is_enable __unused)
{
    return 0;
}

