#ifndef __SOUND_COD3022X_H__
#define __SOUND_COD3022X_H__

#define COD3022X_MIC1	0
#define COD3022X_MIC2	1

int cod3022x_set_externel_jd(struct snd_soc_codec *codec);
int cod3022x_mic_bias_ev(struct snd_soc_codec *codec,int mic, int event);
void cod3022x_process_button_ev(int code, int on);

#endif /* __SOUND_COD3022X_H__ */
