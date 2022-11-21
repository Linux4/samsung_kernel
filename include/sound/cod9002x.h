#ifndef __SOUND_COD3022X_H__
#define __SOUND_COD3022X_H__

#define COD9002X_MICBIAS1	0
#define COD9002X_MICBIAS2	1

extern void cod9002x_call_notifier(int irq1, int irq2, int irq3, int status1);
int cod9002x_mic_bias_ev(struct snd_soc_codec *codec,int mic_bias, int event);

#endif /* __SOUND_COD3022X_H__ */
