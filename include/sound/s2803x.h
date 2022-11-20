#ifndef __SOUND_S2803X_H__
#define __SOUND_S2803X_H__

#include <sound/pcm_params.h>

typedef enum {
	S2803X_IF_AP = 1,
	S2803X_IF_CP,
	S2803X_IF_BT,
	S2803X_IF_ADC,

} s2803x_if_t;

int s2803x_hw_params(struct snd_pcm_substream *,
				struct snd_pcm_hw_params *,
				int bfs, int interface);

void s2803x_startup(s2803x_if_t interface);
void s2803x_shutdown(s2803x_if_t interface);
void s2803x_get_sync(void);
void s2803x_put_sync(void);
void aud_mixer_MCLKO_enable(void);
void aud_mixer_MCLKO_disable(void);

/**
 * is_cp_aud_enabled(void): Checks the current status of CP path
 *
 * Returns true if CP audio path is enabled, false otherwise.
 */
#if defined(CONFIG_SND_SOC_SRC2803X)
bool is_cp_aud_enabled(void);
#else
bool is_cp_aud_enabled(void)
{
	return false;
}
#endif

#endif /* __SOUND_S2803X_H__ */
