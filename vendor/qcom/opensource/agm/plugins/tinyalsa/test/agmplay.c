/*
** Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
**/

#include <errno.h>
#include <tinyalsa/asoundlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <agm/agm_api.h>
#include "agmmixer.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

static int close = 0;

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int device_kv,
                 struct chunk_fmt fmt, struct device_config *dev_config, bool haptics);

void stream_close(int sig)
{
    /* allow the stream to be closed gracefully */
    signal(sig, SIG_IGN);
    close = 1;
}

int main(int argc, char **argv)
{
    FILE *file;
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt;
    unsigned int card = 100, device = 100;
    unsigned int device_kv = 0;
    bool haptics;
    char *intf_name = NULL;
    struct device_config config;
    char *filename;
    int more_chunks = 1, ret = 0;

    if (argc < 3) {
        printf("Usage: %s file.wav [-D card] [-d device] [-i device_id] [-h haptics]",
               " [-dkv device_kv]\n",
                argv[0]);
        return 1;
    }

    filename = argv[1];
    file = fopen(filename, "rb");
    if (!file) {
        printf("Unable to open file '%s'\n", filename);
        return 1;
    }

    fread(&riff_wave_header, sizeof(riff_wave_header), 1, file);
    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        printf("Error: '%s' is not a riff/wave file\n", filename);
        fclose(file);
        return 1;
    }

    do {
        fread(&chunk_header, sizeof(chunk_header), 1, file);

        switch (chunk_header.id) {
        case ID_FMT:
            fread(&chunk_fmt, sizeof(chunk_fmt), 1, file);
            /* If the format header is larger, skip the rest */
            if (chunk_header.sz > sizeof(chunk_fmt))
                fseek(file, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
            break;
        case ID_DATA:
            /* Stop looking for chunks */
            more_chunks = 0;
            break;
        default:
            /* Unknown chunk, skip bytes */
            fseek(file, chunk_header.sz, SEEK_CUR);
        }
    } while (more_chunks);

    /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            if (*argv)
                device = atoi(*argv);
        } else if (strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        } else if (strcmp(*argv, "-i") == 0) {
            argv++;
            if (*argv)
                intf_name = *argv;
        } else if (strcmp(*argv, "-h") == 0) {
            argv++;
            if (*argv)
                haptics = *argv;
        } else if (strcmp(*argv, "-dkv") == 0) {
            argv++;
            if (*argv)
                device_kv = convert_char_to_hex(*argv);
        }
        if (*argv)
            argv++;
    }

    if (intf_name == NULL)
        return 1;

    ret = get_device_media_config(BACKEND_CONF_FILE, intf_name, &config);
    if (ret) {
        printf("Invalid input, entry not found for : %s\n", intf_name);
        fclose(file);
        return ret;
    }
    play_sample(file, card, device, device_kv, chunk_fmt, &config, haptics);

    fclose(file);

    return 0;
}

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int device_kv,
                 struct chunk_fmt fmt, struct device_config *dev_config, bool haptics)
{
    struct pcm_config config;
    struct pcm *pcm;
    struct mixer *mixer;
    char *buffer;
    int playback_path, playback_value;
    int size;
    int num_read;
    char *name = dev_config->name;
    struct group_config grp_config;

    memset(&config, 0, sizeof(config));
    config.channels = fmt.num_channels;
    config.rate = fmt.sample_rate;
    config.period_size = 1024;
    config.period_count = 4;
    if (fmt.bits_per_sample == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (fmt.bits_per_sample == 24)
        config.format = PCM_FORMAT_S24_3LE;
    else if (fmt.bits_per_sample == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    printf("Backend %s rate ch bit : %d, %d, %d\n", name,
            dev_config->rate, dev_config->ch, dev_config->bits);
    mixer = mixer_open(card);
    if (!mixer) {
        printf("Failed to open mixer\n");
        return;
    }

    /* set device/audio_intf media config mixer control */
    if (set_agm_device_media_config(mixer, dev_config->ch, dev_config->rate,
                                    dev_config->bits, name)) {
        printf("Failed to set device media config\n");
        goto err_close_mixer;
    }

    if (haptics) {
        playback_path = HAPTICS;
        playback_value = HAPTICS_PLAYBACK;
    } else {
        playback_path = PLAYBACK;
        playback_value = PCM_LL_PLAYBACK;
    }
     /* set audio interface metadata mixer control */
    if (set_agm_audio_intf_metadata(mixer, name, device_kv, playback_path,
                                    dev_config->rate, dev_config->bits, PCM_LL_PLAYBACK)) {
        printf("Failed to set device metadata\n");
        goto err_close_mixer;
    }
    /* set audio interface metadata mixer control */
    if (set_agm_stream_metadata(mixer, device, playback_value, PLAYBACK, STREAM_PCM, NULL)) {
        printf("Failed to set pcm metadata\n");
        goto err_close_mixer;
    }

    /* Note:  No common metadata as of now*/

    /* connect pcm stream to audio intf */
    if (connect_agm_audio_intf_to_stream(mixer, device, name, STREAM_PCM, true)) {
        printf("Failed to connect pcm to audio interface\n");
        goto err_close_mixer;
    }

    if (configure_mfc(mixer, device, name, PER_STREAM_PER_DEVICE_MFC,
                           STREAM_PCM, dev_config->rate, dev_config->ch,
                           dev_config->bits)) {
        printf("Failed to configure pspd mfc\n");
        goto err_close_mixer;
    }

    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        printf("Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        goto err_close_mixer;
    }

    if (strstr(name, "VIRT-")) {
        if (get_group_device_info(BACKEND_CONF_FILE, name, &grp_config))
            goto err_close_mixer;

        if (set_agm_group_device_config(mixer, device, &grp_config, name)) {
            printf("Failed to set grp device config\n");
            goto err_close_mixer;
        }
    }

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        printf("Unable to allocate %d bytes\n", size);
        goto err_close_pcm;
    }

    printf("Playing sample: %u ch, %u hz, %u bit\n", fmt.num_channels,
            fmt.sample_rate, fmt.bits_per_sample);

    if (pcm_start(pcm) < 0) {
        printf("start error\n");
        goto err_close_pcm;
    }

    /* catch ctrl-c to shutdown cleanly */
    signal(SIGINT, stream_close);

    do {
        num_read = fread(buffer, 1, size, file);
        if (num_read > 0) {
            if (pcm_write(pcm, buffer, num_read)) {
                printf("Error playing sample\n");
                break;
            }
        }
    } while (!close && num_read > 0);

    pcm_stop(pcm);
    /* connect pcm stream to audio intf */
    connect_agm_audio_intf_to_stream(mixer, device, name, STREAM_PCM, false);

    free(buffer);
err_close_pcm:
    pcm_close(pcm);
err_close_mixer:
    mixer_close(mixer);
}

