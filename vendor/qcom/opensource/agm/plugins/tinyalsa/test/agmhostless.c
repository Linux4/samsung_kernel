/*
** Copyright (c) 2019, The Linux Foundation. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above
**     copyright notice, this list of conditions and the following
**     disclaimer in the documentation and/or other materials provided
**     with the distribution.
**   * Neither the name of The Linux Foundation nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
** ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
** IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include "agmmixer.h"

static int close_f = 0;

void sigint_handler(int sig)
{
    close_f = 1;
}

char *capture_audio_interface_name[] = {
    "CODEC_DMA-LPAIF_VA-TX-0",
    "CODEC_DMA-LPAIF_VA-TX-1",
    "MI2S-LPAIF_AXI-TX-PRIMARY",
    "TDM-LPAIF_AXI-TX-PRIMARY",
    "AUXPCM-LPAIF_AXI-TX-PRIMARY",
};

char *playback_audio_interface_name[] = {
    "CODEC_DMA-LPAIF_WSA-RX-0",
    "CODEC_DMA-LPAIF_WSA-RX-1",
    "MI2S-LPAIF_AXI-RX-PRIMARY",
    "TDM-LPAIF_AXI-RX-PRIMARY",
    "AUXPCM-LPAIF_AXI-RX-PRIMARY",
};

static void play_loopback(unsigned int card, unsigned int p_device, unsigned int c_device, unsigned int channels,
                 unsigned int rate, enum pcm_format format, unsigned int period_size,
                 unsigned int period_count, unsigned int play_cap_time, unsigned int capture_intf, unsigned int play_intf);

int main(int argc, char **argv)
{
    unsigned int card = 100;
    unsigned int p_device = 1;
    unsigned int c_device = 0;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;
    unsigned int bits = 16;
    unsigned int num_channels = 2;
    unsigned int sample_rate = 48000;
    unsigned int play_cap_time = 10;
    unsigned int c_audio_intf = 0;
    unsigned int p_audio_intf = 0;
    enum pcm_format format;

    if (argc < 2) {
        printf("Usage: %s [-D card] [-P Hostless Playback device] [-C Hostless Capture device] [-p period_size]"
                " [-n n_periods]  [-c num_channels]  [-r sample_rate]   [-T playback/capture time ]  [-i capture intf id] [-o playback intf id]\n"
               " valid playback_intf_id :\n"
                "0 : WSA_CDC_DMA_RX_0\n"
                "1 : RX_CDC_DMA_RX_0\n"
                "2 : SLIM_0_RX\n"
                "3 : DISPLAY_PORT_RX\n"
                "4 : PRI_TDM_RX_0\n"
                "5 : SEC_TDM_RX_0\n"
                "6 : AUXPCM_RX\n"
                "7 : SEC_AUXPCM_RX\n"
                "8 : PRI_MI2S_RX\n"
                "9 : SEC_MI2S_RX\n"
                " valid capture_intf_id :\n"
                "0 : TX_CDC_DMA_TX_0\n"
                "1 : SLIM_0_TX\n"
                "2 : PRI_TDM_TX_0\n"
                "3 : SEC_TDM_TX_0\n"
                "4 : AUXPCM_TX\n"
                "5 : SEC_AUXPCM_TX\n"
                "6 : PRI_MI2S_TX\n"
                "7 : SEC_MI2S_TX\n",
                 argv[0]);
        return 1;
    }

   /* parse command line arguments */
    argv += 1;
    while (*argv) {
        if (strcmp(*argv, "-P") == 0) {
            argv++;
            if (*argv)
                p_device = atoi(*argv);
        }
	 if (strcmp(*argv, "-C") == 0) {
            argv++;
            if (*argv)
                c_device = atoi(*argv);
        }
        if (strcmp(*argv, "-p") == 0) {
            argv++;
            if (*argv)
                period_size = atoi(*argv);
        }
        if (strcmp(*argv, "-n") == 0) {
            argv++;
            if (*argv)
                period_count = atoi(*argv);
        }
	if (strcmp(*argv, "-c") == 0) {
            argv++;
            if (*argv)
                num_channels = atoi(*argv);
        }
	if (strcmp(*argv, "-r") == 0) {
            argv++;
            if (*argv)
                sample_rate = atoi(*argv);
        }

	if (strcmp(*argv, "-T") == 0) {
            argv++;
            if (*argv)
                play_cap_time = atoi(*argv);
        }

        if (strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        }
        if (strcmp(*argv, "-i") == 0) {
            argv++;
            if (*argv)
                c_audio_intf = atoi(*argv);
            if (c_audio_intf >= sizeof(capture_audio_interface_name)/sizeof(char *)) {
                printf("Invalid audio interface index denoted by -i\n");
                return 1;
            }
        }
        if (strcmp(*argv, "-o") == 0) {
            argv++;
            if (*argv)
                p_audio_intf = atoi(*argv);
            if (p_audio_intf >= sizeof(playback_audio_interface_name)/sizeof(char *)) {
                printf("Invalid audio interface index denoted by -i\n");
                return 1;
            }
        }

        if (*argv)
            argv++;
    }

    switch (bits) {
    case 32:
        format = PCM_FORMAT_S32_LE;
        break;
    case 24:
        format = PCM_FORMAT_S24_LE;
        break;
    case 16:
        format = PCM_FORMAT_S16_LE;
        break;
    default:
        printf("%u bits is not supported.\n", bits);
        return 1;
    }

    signal(SIGINT, sigint_handler);
    signal(SIGHUP, sigint_handler);
    signal(SIGTERM, sigint_handler);

    play_loopback(card, p_device, c_device, num_channels, sample_rate,
                  format, period_size, period_count, play_cap_time,
                  c_audio_intf, p_audio_intf);

    return 0;
}

void play_loopback(unsigned int card, unsigned int p_device, unsigned int c_device, unsigned int channels,
                 unsigned int rate, enum pcm_format format, unsigned int period_size,
                 unsigned int period_count, unsigned int play_cap_time, unsigned int capture_intf, unsigned int play_intf)
{
    struct pcm_config config;
    struct pcm *p_pcm, *c_pcm;
    struct mixer *mixer;
    char *buffer;
    char *p_intf_name = playback_audio_interface_name[play_intf];
    char *c_intf_name = capture_audio_interface_name[capture_intf];
    unsigned int size;
    unsigned int bytes_read = 0;
    unsigned int frames = 0;
    struct timespec end;
    struct timespec now;

    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    config.format = format;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    mixer = mixer_open(card);
    if (!mixer) {
        printf("Failed to open mixer\n");
        return;
    }

    /* set device/audio_intf media config mixer control */
    if (set_agm_device_media_config(mixer, channels, rate,
                                    pcm_format_to_bits(format), p_intf_name)) {
        printf("Failed to set playback device media config\n");
        goto err_close_mixer;
    }

    if (set_agm_device_media_config(mixer, channels, rate,
                                    pcm_format_to_bits(format), c_intf_name)) {
        printf("Failed to set capture device media config\n");
        goto err_close_mixer;
    }

    /* set audio interface metadata mixer control */
    if (set_agm_audio_intf_metadata(mixer, c_intf_name, 0, CAPTURE, rate, pcm_format_to_bits(format), PCM_RX_LOOPBACK)) {
        printf("Failed to set capture device metadata\n");
        goto err_close_mixer;
    }

    if (set_agm_audio_intf_metadata(mixer, p_intf_name, 0, PLAYBACK, rate, pcm_format_to_bits(format), PCM_RX_LOOPBACK)) {
        printf("Failed to set playback device metadata\n");
        goto err_close_mixer;
    }

    if (set_agm_stream_metadata(mixer, c_device, PCM_RX_LOOPBACK, LOOPBACK, STREAM_PCM, NULL)) {
        printf("Failed to capture stream metadata\n");
        goto err_close_mixer;
    }

    /* Note:  No common metadata as of now*/

    /* connect pcm stream to audio intf */
    if (connect_agm_audio_intf_to_stream(mixer, p_device, p_intf_name, STREAM_PCM, true)) {
        printf("Failed to connect playback pcm to audio interface\n");
        goto err_close_mixer;
    }

    if (connect_agm_audio_intf_to_stream(mixer, c_device, c_intf_name, STREAM_PCM, true)) {
        printf("Failed to connect capture pcm to audio interface\n");
        connect_agm_audio_intf_to_stream(mixer, p_device, p_intf_name, STREAM_PCM, false);
        goto err_close_mixer;
    }

    if (connect_play_pcm_to_cap_pcm(mixer, p_device, c_device)) {
        printf("Failed to connect capture pcm to audio interface\n");
        goto err_disconnect;
    }

    p_pcm = pcm_open(card, p_device, PCM_OUT, &config);
    if (!p_pcm || !pcm_is_ready(p_pcm)) {
        printf("Unable to open playback PCM device (%s)\n",
                pcm_get_error(p_pcm));
        goto err_close_c_pcm;
    }

    if (pcm_start(p_pcm) < 0) {
        printf("start error");
        goto err_close_p_pcm;
    }

    c_pcm = pcm_open(card, c_device, PCM_IN, &config);
    if (!c_pcm || !pcm_is_ready(c_pcm)) {
        printf("Unable to open playback PCM device (%s)\n",
                pcm_get_error(c_pcm));
        goto err_disconnect;
    }

    if (pcm_start(c_pcm) < 0) {
        printf("start error");
        goto err_close_c_pcm;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    end.tv_sec += play_cap_time;
    while (1) {
        if (close_f)
            break;

        if (play_cap_time) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec > end.tv_sec ||
                (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
                break;
        }
    }
    connect_play_pcm_to_cap_pcm(mixer, -1, c_device);
err_close_p_pcm:
    pcm_close(p_pcm);
err_close_c_pcm:
    pcm_close(c_pcm);
err_disconnect:
    connect_agm_audio_intf_to_stream(mixer, p_device, p_intf_name, STREAM_PCM, false);
    connect_agm_audio_intf_to_stream(mixer, c_device, c_intf_name, STREAM_PCM, false);
err_close_mixer:
    mixer_close(mixer);
}
