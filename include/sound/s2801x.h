#ifndef __SOUND_S2801X_H__
#define __SOUND_S2801X_H__

#include <sound/pcm_params.h>

typedef enum {
	S2801X_IF_AP = 1,
	S2801X_IF_CP,
	S2801X_IF_BT,
	S2801X_IF_ADC,
} s2801x_if_t;

#if defined(CONFIG_SND_SOC_SRC2801X)
/**
 * s2801x_hw_params(): Function to configure the mixer interfaces. This function
 * is generally called from the sound-card driver's shutdown() hook.
 *
 * substream, params: These parameters are passed from ALSA core, the calling
 * function passes the same arguments to this function.
 * bfs: The required BFS value (as decided by the calling function)
 * interface: The ID of the interface that needs to be updated
 *
 * Returns 0 on success, else an error code.
 */
int s2801x_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				int bfs, int interface);

/**
 * s2801x_startup(): Called before actual mixer operation starts. This function
 * is generally called from the sound-card driver's startup() hook.
 *
 * interface: The ID of the target interface
 */
void s2801x_startup(s2801x_if_t interface);

/**
 * s2801x_shutdown(): Called after the end of mixer operations. This function is
 * generally called from the sound-card driver's shutdown() hook.
 *
 * interface: The ID of the target interface
 */
void s2801x_shutdown(s2801x_if_t interface);

/**
 * s2801x_get_sync(): Call runtime resume function of mixer. The codec driver
 * requires the mixer driver to be operational before it gets configured. The
 * codec driver can make use of this API to power on the mixer driver before
 * starting its own resume sequence.
 */
void s2801x_get_sync(void);

/**
 * s2801x_put_sync(): Call runtime suspend function of mixer. This function is
 * the counterpart of s2801x_put_sync() function and it is designed to balance
 * the resume call made through s2801x_get_sync().
 */
void s2801x_put_sync(void);

/**
 * is_cp_aud_enabled(): Checks the current status of CP path
 *
 * Returns true if CP audio path is enabled, false otherwise.
 */
bool is_cp_aud_enabled(void);

/**
 * is_cp_voice_call(): Checks whether CP call is active
 *
 * Returns true if CP voice call is active, false otherwise
 */
bool is_cp_voice_call(void);
#else
int s2801x_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				int bfs, int interface)
{
}

void s2801x_startup(s2801x_if_t interface)
{
}

void s2801x_shutdown(s2801x_if_t interface)
{
}

void s2801x_get_sync(void)
{
}

void s2801x_put_sync(void)
{
}

bool is_cp_aud_enabled(void)
{
	return false;
}

bool is_cp_voice_call()
{
	return false;
}
#endif

#endif /* __SOUND_S2801X_H__ */
