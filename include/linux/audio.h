/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_AUDIO_H
#define _LINUX_AUDIO_H

/*
 *	earjac interface functions
 */
struct audiodevice  {
	struct device *chardev;
	struct snd_soc_jack *jack;
};

extern int register_audio_earjack(struct audiodevice *audio);
extern void unregister_audio_earjack(struct audiodevice *audio);

#endif /* _LINUX_AUDIO_H */
