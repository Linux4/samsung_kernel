/*
 *  sec_audio_debug.c
 *
 *  Copyright (c) 2018 Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _SEC_AUDIO_DEBUG_H
#define _SEC_AUDIO_DEBUG_H

#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
int is_abox_rdma_enabled(int id);
int is_abox_wdma_enabled(int id);
void abox_debug_string_update(void);

int register_debug_mixer(struct snd_soc_card *card);
void sec_audio_log(int level, const char *fmt, ...);
int alloc_sec_audio_log(size_t buffer_len);
void free_sec_audio_log(void);
void sec_audio_bootlog(int level, const char *fmt, ...);
int alloc_sec_audio_bootlog(size_t buffer_len);
void free_sec_audio_bootlog(void);

#define adev_err(dev, fmt, arg...) \
do { \
	dev_err(dev, fmt, ##arg); \
	sec_audio_log(3, fmt, ##arg); \
} while (0)

#define adev_warn(dev, fmt, arg...) \
do { \
	dev_warn(dev, fmt, ##arg); \
	sec_audio_log(4, fmt, ##arg); \
} while (0)

#define adev_info(dev, fmt, arg...) \
do { \
	dev_info(dev, fmt, ##arg); \
	sec_audio_log(6, fmt, ##arg); \
} while (0)

#define adev_dbg(dev, fmt, arg...) \
do { \
	dev_dbg(dev, fmt, ##arg); \
	sec_audio_log(7, fmt, ##arg); \
} while (0)

#define bdev_err(dev, fmt, arg...) \
do { \
	dev_err(dev, fmt, ##arg); \
	sec_audio_bootlog(3, fmt, ##arg); \
} while (0)

#define bdev_warn(dev, fmt, arg...) \
do { \
	dev_warn(dev, fmt, ##arg); \
	sec_audio_bootlog(4, fmt, ##arg); \
} while (0)

#define bdev_info(dev, fmt, arg...) \
do { \
	dev_info(dev, fmt, ##arg); \
	sec_audio_bootlog(6, fmt, ##arg); \
} while (0)

#define bdev_dbg(dev, fmt, arg...) \
do { \
	dev_dbg(dev, fmt, ##arg); \
	sec_audio_bootlog(7, fmt, ##arg); \
} while (0)

#ifdef CHANGE_DEV_PRINT
#ifdef dev_err
#undef dev_err
#endif
#define dev_err(dev, fmt, arg...) \
do { \
	dev_printk(KERN_ERR, dev, fmt, ##arg); \
	sec_audio_log(3, fmt, ##arg); \
} while (0)

#ifdef dev_warn
#undef dev_warn
#endif
#define dev_warn(dev, fmt, arg...) \
do { \
	dev_printk(KERN_WARNING, dev, fmt, ##arg); \
	sec_audio_log(4, fmt, ##arg); \
} while (0)

#ifdef dev_info
#undef dev_info
#endif
#define dev_info(dev, fmt, arg...) \
do { \
	dev_printk(KERN_INFO, dev, fmt, ##arg); \
	sec_audio_log(6, fmt, ##arg); \
} while (0)

#ifdef DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg(dev, fmt, arg...) \
do { \
	dev_printk(KERN_DEBUG, dev, fmt, ##arg); \
	sec_audio_log(7, fmt, ##arg); \
} while (0)
#endif /* DEBUG */

#endif /* CHANGE_DEV_PRINT */

#else /* CONFIG_SND_SOC_SAMSUNG_AUDIO */
inline int is_abox_rdma_enabled(int id)
{
	return 0;
}
inline int is_abox_wdma_enabled(int id)
{
	return 0;
}
inline void abox_gpr_string_update(void)
{}


int register_debug_mixer(struct snd_soc_card *card)
{
	return -EACCES;
}

inline void sec_audio_log(int level, const char *fmt, ...)
{
}

inline int alloc_sec_audio_log(size_t buffer_len)
{
	return -EACCES;
}

inline void free_sec_audio_log(void)
{
}

inline void sec_audio_bootlog(int level, const char *fmt, ...)
{
}

inline int alloc_sec_audio_bootlog(size_t buffer_len)
{
	return -EACCES;
}

inline void free_sec_audio_bootlog(void)
{
}

#define adev_err(dev, fmt, arg...) dev_err(dev, fmt, ##arg)
#define adev_warn(dev, fmt, arg...) dev_warn(dev, fmt, ##arg)
#define adev_info(dev, fmt, arg...) dev_info(dev, fmt, ##arg)
#define adev_dbg(dev, fmt, arg...) dev_dbg(dev, fmt, ##arg)

#define bdev_err(dev, fmt, arg...) dev_err(dev, fmt, ##arg)
#define bdev_warn(dev, fmt, arg...) dev_warn(dev, fmt, ##arg)
#define bdev_info(dev, fmt, arg...) dev_info(dev, fmt, ##arg)
#define bdev_dbg(dev, fmt, arg...) dev_dbg(dev, fmt, ##arg)

#endif /* CONFIG_SND_SOC_SAMSUNG_AUDIO */

#endif
