#ifndef _SEC_AUDIO_SYSFS_H
#define _SEC_AUDIO_SYSFS_H

struct sec_audio_sysfs_data {
	struct class *audio_class;

#ifdef CONFIG_SND_SOC_SAMSUNG_SYSFS_EARJACK
	struct device *jack_dev;
	dev_t jack_dev_id;
	int (*get_jack_state)(void);
	int (*get_key_state)(void);
	int (*get_mic_adc)(void);
#endif
};

#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
#ifdef CONFIG_SND_SOC_SAMSUNG_SYSFS_EARJACK
int audio_register_jack_state_cb(int (*jack_status) (void));
int audio_register_key_state_cb(int (*key_state) (void));
int audio_register_mic_adc_cb(int (*mic_adc) (void));
#endif /* CONFIG_SND_SOC_SAMSUNG_SYSFS_EARJACK */
#else /* CONFIG_SND_SOC_SAMSUNG_AUDIO */
#ifdef CONFIG_SND_SOC_SAMSUNG_SYSFS_EARJACK
inline int audio_register_jack_state_cb(int (*jack_status) (void))
{
	return -EACCES;
}

inline int audio_register_key_state_cb(int (*key_state) (void))
{
	return -EACCES;
}

inline int audio_register_mic_adc_cb(int (*mic_adc) (void))
{
	return -EACCES;
}
#endif /* CONFIG_SND_SOC_SAMSUNG_SYSFS_EARJACK */
#endif /* CONFIG_SND_SOC_SAMSUNG_AUDIO */
#endif /* _SEC_AUDIO_SYSFS_H */
