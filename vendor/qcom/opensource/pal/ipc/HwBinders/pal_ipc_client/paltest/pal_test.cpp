/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#define LOG_TAG "paltest"

#include <log/log.h>
#include <stdio.h>
#include <stdlib.h>
#include "PalApi.h"

int main()
{
    int ret = 0;
    ALOGE("calling stream_open\n");
    struct pal_stream_attributes attr;
    struct pal_channel_info temp;
    pal_stream_handle_t *pal_stream_handle;
    uint16_t channels = 3;
    struct modifier_kv mod_kv[2] = {{1,2},{9,11}};
    struct pal_device *devices;
    uint32_t noOfDevice = 2;
    temp.channels = channels;
    switch (channels) {
        case 1:
              temp.ch_map[0] = 1;
              break;
        case 2:
              temp.ch_map[0] = 1;
              temp.ch_map[1] = 3;
              break;
        case 3:
              temp.ch_map[0] = 1;
              temp.ch_map[1] = 3;
              temp.ch_map[2] = 5;
              break;
         case 4:
              temp.ch_map[0] = 1;
              temp.ch_map[1] = 3;
              temp.ch_map[2] = 5;
              temp.ch_map[3] = 7;
              break;
    }
    devices = (struct pal_device*) calloc (1, sizeof(struct pal_device)*noOfDevice);
    for (int i =0; i < noOfDevice; i++) {
        devices[i].id = PAL_DEVICE_OUT_WIRED_HEADPHONE;
        devices[i].config.sample_rate =  48000;
        devices[i].config.bit_width =  16;
        devices[i].config.aud_fmt_id =  PAL_AUDIO_FMT_DEFAULT_PCM;
        devices[i].config.ch_info =  temp;
    }
    attr.type =  PAL_STREAM_LOOPBACK;
    attr.info.opt_stream_info = {1234, 64, 12345678, 0, 1, 0};
    attr.flags = (pal_stream_flags_t)(PAL_STREAM_FLAG_TIMESTAMP | PAL_STREAM_FLAG_NON_BLOCKING);
    attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    attr.in_media_config.ch_info = temp;
    attr.in_media_config.sample_rate = 48000;
    attr.in_media_config.bit_width = 24;
    attr.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_PCM;
    attr.out_media_config.ch_info = temp;
    attr.out_media_config.sample_rate = 96000;
    attr.out_media_config.bit_width = 16;
    attr.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_DEFAULT_PCM;

    ret = pal_stream_open(&attr, noOfDevice, devices, (uint32_t)2, mod_kv, NULL, NULL, &pal_stream_handle);

    return 0;
}
