#ifndef __SPRD_EIRQSOFF_H
#define __SPRD_EIRQSOFF_H

#ifdef CONFIG_SPRD_EIRQSOFF_DEBUG
extern void notrace start_eirqsoff_timing(void);
extern void notrace stop_eirqsoff_timing(void);
#else
#define start_eirqsoff_timing()            do { } while (0)
#define stop_eirqsoff_timing()           do { } while (0)
#endif

#endif
