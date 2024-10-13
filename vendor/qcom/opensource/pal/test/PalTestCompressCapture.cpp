/**
 * Changes from Qualcomm Innovation Center are provided under the following
 * license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#include <PalApi.h>
#include <PalDefs.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_map>

#ifdef __ANDROID__
#include <log/log.h>
#endif

#ifndef __PAL_TEST_COMPRESS_CAPTURE_LOG__
#define __PAL_TEST_COMPRESS_CAPTURE_LOG__

#ifndef __ANDROID__
#define ALOGD(...) ((void)0)
#define ALOGE(...) ((void)0)
#endif

#define CCLOGD(arg, ...)                                                  \
    ALOGD("%s: LINE: %d " arg, __func__, __LINE__, ##__VA_ARGS__);        \
    fprintf(stdout, "%s: %s: LINE: %d " arg "\n", PROGRAM_NAME, __func__, \
            __LINE__, ##__VA_ARGS__);

#define CCLOGE(arg, ...)                                                  \
    ALOGE("%s: LINE: %d " arg, __func__, __LINE__, ##__VA_ARGS__);        \
    fprintf(stderr, "%s: %s: LINE: %d " arg "\n", PROGRAM_NAME, __func__, \
            __LINE__, ##__VA_ARGS__);

#endif

static char* PROGRAM_NAME = "";

// LIST OF MIC's
static const std::unordered_map<char*, pal_device_id_t> sMicList = {
    {"handset_mic", PAL_DEVICE_IN_HANDSET_MIC},
    {"speaker_mic", PAL_DEVICE_IN_SPEAKER_MIC},
    {"headset_mic", PAL_DEVICE_IN_WIRED_HEADSET}};

/**
 * output path is /data/<OUTPUT_FILENAME>
 **/
static char* OUTPUT_FILENAME = nullptr;

/**
 * As of now only supports AAC
 * TODO validation
 **/
static char* AUDIOFORMAT = nullptr;

/**
 * encoder mode
 * 0x2  -> 2  AAC_AOT_LC
 * 0x5  -> 5  AAC_AOT_SBR
 * 0x1d -> 29 AAC_AOT_PS
 *
 * format flag
 * 0x0  -> 0 AAC_FORMAT_FLAG_ADTS
 * 0x1  -> 1 AAC_FORMAT_FLAG_LOAS
 * 0x3  -> 3 AAC_FORMAT_FLAG_RAW
 * 0x4  -> 4 AAC_FORMAT_FLAG_LATM
 **/
static int32_t ENCODE_MODE = 0x02;  // default AAC LC
static int32_t FORMAT_FLAG = 0;     // default AAC with ADTS

/**
 * AAC supported sample rates 8000 11025 12000 16000 22050 24000 32000 44100
 *48000 AAC supported channel count 1,2 AAC supported bitrate ?depedent on
 *sample rate and channelcount TODO validation on bitrate
 **/
static int32_t SAMPLE_RATE = 48000;  // default
static int32_t BIT_RATE = 64000;     // default
static int32_t CHANNEL_COUNT = 2;    // default

static int64_t TIME_TO_READ_IN_MILLI = 30000;  // default

static pal_device_id_t MIC_TYPE = PAL_DEVICE_IN_HANDSET_MIC;  // default
static char* MIC_NAME = nullptr;

int run_compress_capture_usecase() {
    pal_stream_handle_t* pal_stream;

    std::string filePath("/data/");
    filePath.append(OUTPUT_FILENAME);

    int fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        CCLOGE("open failed");
        return errno;
    }

    CCLOGD("filepath is %s", filePath.c_str());

    // sAACWriter= new AACWriter(fd);
    // sAACWriter->set(2,48000,0x02);

    int32_t status = 0;
    // setting stream attributes
    auto stream_attributes = std::make_unique<struct pal_stream_attributes>();

    CCLOGD("given sample rate is %d; channel count is %d; bitwidth is 16",
           SAMPLE_RATE, CHANNEL_COUNT);
    stream_attributes->type = PAL_STREAM_COMPRESSED;
    stream_attributes->direction = PAL_AUDIO_INPUT;
    stream_attributes->in_media_config.sample_rate = SAMPLE_RATE;
    stream_attributes->in_media_config.bit_width =
        16;  // AAC encoder supports only 16
    stream_attributes->in_media_config.aud_fmt_id = PAL_AUDIO_FMT_AAC;
    stream_attributes->in_media_config.ch_info.channels = CHANNEL_COUNT;
    stream_attributes->in_media_config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    stream_attributes->in_media_config.ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;

    // setting device attriutes
    auto pal_devices = std::make_unique<struct pal_device>();
    pal_devices->id = MIC_TYPE;

    status = pal_stream_open(stream_attributes.get(), 1, pal_devices.get(), 0,
                             NULL, NULL, 0, &pal_stream);

    if (status) {
        CCLOGE("pal_stream_open failed");
        return -EINVAL;
    }
    CCLOGD("pal_stream_open successful");

    // pal buffer config
    pal_buffer_config_t in_buf_conf;
    in_buf_conf.buf_count = 4;
    in_buf_conf.buf_size = 2048;
    status = pal_stream_set_buffer_size(pal_stream, &in_buf_conf, NULL);
    if (status) {
        CCLOGE("pal_stream_set_buffer_size failed");
        return pal_stream_close(pal_stream);
    }
    CCLOGD(
        "pal_stream_set_buffer_size successful; ring buffer size is "
        "4*2048bytes");

    // payload
    auto encoder_payload = std::make_unique<uint8_t[]>(
        sizeof(pal_param_payload) + sizeof(pal_snd_enc_t));

    auto payload_wrap =
        reinterpret_cast<pal_param_payload*>(encoder_payload.get());
    payload_wrap->payload_size = sizeof(pal_snd_enc_t);

    auto encoder_config = reinterpret_cast<pal_snd_enc_t*>(
        encoder_payload.get() + sizeof(pal_param_payload));
    CCLOGD("ENCODE MODE: 0x%02x, FORMAT FLAG: 0x%02x, BITRATE:%d", ENCODE_MODE,
           FORMAT_FLAG, BIT_RATE);
    encoder_config->aac_enc.enc_cfg.aac_enc_mode = ENCODE_MODE;
    encoder_config->aac_enc.enc_cfg.aac_fmt_flag = FORMAT_FLAG;
    encoder_config->aac_enc.aac_bit_rate = BIT_RATE;

    status = pal_stream_set_param(pal_stream, PAL_PARAM_ID_CODEC_CONFIGURATION,
                                  payload_wrap);

    if (status) {
        CCLOGE("pal_stream_set_param failed");
        return pal_stream_close(pal_stream);
    }
    CCLOGD("pal_stream_set_param successful");

    status = pal_stream_start(pal_stream);
    if (status) {
        CCLOGE("pal_stream_start failed");
        return pal_stream_close(pal_stream);
    }
    CCLOGD("pal_stream_start successful");

    // pal buffer
    auto dataBuffer = std::make_unique<uint8_t[]>(2048 * sizeof(uint8_t));
    ssize_t readBytes = 0;
    struct pal_buffer palBuffer;
    palBuffer.buffer = dataBuffer.get();
    palBuffer.size = 2048;
    palBuffer.offset = 0;

    auto start = std::chrono::steady_clock::now();
    for (; std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(TIME_TO_READ_IN_MILLI);) {
        memset(palBuffer.buffer, 0, palBuffer.size);
        readBytes = pal_stream_read(pal_stream, &palBuffer);
        if (readBytes <= 0) {
            CCLOGE("pal_stream_read failure");
            return pal_stream_close(pal_stream);
        }
        // sAACWriter->writeAACFrame(palBuffer.buffer, readBytes);
        write(fd, palBuffer.buffer, readBytes);
        CCLOGD("pal_stream_read: %zu size ", readBytes);
    }

    status = pal_stream_stop(pal_stream);
    if (status) {
        CCLOGE("pal_stream_stop failed");
        return pal_stream_close(pal_stream);
    }
    CCLOGD("pal_stream_stop successful");

    status = pal_stream_close(pal_stream);
    if (status) {
        CCLOGE("pal_stream_close failed");
    }
    CCLOGD("pal_stream_close successful");

    pal_stream = nullptr;
    return status;
}

void print_usage(FILE* stream, int exit_code) {
    fprintf(stream, "Usage:  %s options\n", PROGRAM_NAME);
    fprintf(stream,
            "  -h  Display usage\n"
            "  -a  audioFormat like AAC\n"
            "  -e  encodeMode \n"
            "  -f  formatType      \n"
            "  -s  sampleRate like 48000\n"
            "  -c  channelCount like 1 or 2\n"
            "  -b  bitRate like 64000\n"
            "  -n  fileName like test_clip\n"
            "  -t  timeToReadInMilli like 33000\n"
            "  -m  micType like handset_mic,speaker_mic,headset_mic");
    exit(exit_code);
}

int main(int argc, char* argv[]) {
    PROGRAM_NAME = argv[0];
    int next_option = 0;
    auto short_options = "a:e:f:s:c:b:n:t:m:";
    struct option long_options[] = {
        {"audioFormat", 1, NULL, 'a'},  {"encodeMode", 0, NULL, 'e'},
        {"formatType", 0, NULL, 'f'},   {"sampleRate", 0, NULL, 's'},
        {"channelCount", 0, NULL, 'c'}, {"bitRate", 0, NULL, 'b'},
        {"fileName", 1, NULL, 'n'},     {"timeToReadInMilli", 1, NULL, 't'},
        {"micType", 0, NULL, 'm'},      {NULL, 0, NULL, 0}};

    do {
        next_option =
            getopt_long(argc, argv, short_options, long_options, NULL);
        switch (next_option) {
            case 'h':
                print_usage(stdout, 0);
                break;

            case 'n':
                OUTPUT_FILENAME = optarg;
                break;

            case 'a':
                AUDIOFORMAT = optarg;
                break;

            case 'm':
                MIC_NAME = optarg;
                break;

            case 'e':
                ENCODE_MODE = atoi(optarg);
                break;

            case 'f':
                FORMAT_FLAG = atoi(optarg);
                break;

            case 's':
                SAMPLE_RATE = atoi(optarg);
                break;

            case 'c':
                CHANNEL_COUNT = atoi(optarg);
                break;

            case 'b':
                BIT_RATE = atoi(optarg);
                break;

            case 't':
                TIME_TO_READ_IN_MILLI = atoi(optarg);
                break;

            case '?':
                print_usage(stderr, 1);

            case -1: /* Done with options.  */
                break;

            default:
                abort();
        }
    } while (next_option != -1);

    if (OUTPUT_FILENAME != nullptr) {
        CCLOGD("given filename is %s", OUTPUT_FILENAME);
    } else {
        CCLOGE("no filename is given");
        abort();
    }

    if (TIME_TO_READ_IN_MILLI > 0) {
        CCLOGD("time to record in ms: %d", TIME_TO_READ_IN_MILLI);
    } else {
        CCLOGE("no time in ms is given");
        abort();
    }

    if (AUDIOFORMAT != nullptr) {
        CCLOGD("requested audio format is %s", AUDIOFORMAT);
    } else {
        CCLOGE("no requested audio format");
        abort();
    }

    if (MIC_NAME != nullptr) {
        for (auto mic : sMicList) {
            if (!strcmp(MIC_NAME, mic.first)) {
                MIC_TYPE = mic.second;
                CCLOGD("assigned mic is %s", mic.first);
            }
        }
    }

    return run_compress_capture_usecase();
}