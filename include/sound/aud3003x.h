#ifndef __SOUND_AUD3003X_H__
#define __SOUND_AUD3003X_H__

#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
extern bool aud3003x_notifier_check(void);
extern void aud3003x_call_notifier(void);
#endif
#endif /* __SOUND_AUD3003X_H__ */
