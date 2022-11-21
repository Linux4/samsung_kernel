#ifndef _DBMD2_H
#define _DBMD2_H

int dbmd2_get_frames(char *buffer, unsigned int channels, unsigned int frames);
int dbmd2_codec_lock(void);
void dbmd2_start_buffering(void);
void dbmd2_stop_buffering(void);
int dbmd2_codec_unlock(void);

#endif /* _DBMD2_H */
