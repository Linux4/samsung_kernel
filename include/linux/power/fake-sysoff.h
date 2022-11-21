#ifndef __FAKE_SYSOFF_H__
#define __FAKE_SYSOFF_H__

#ifdef CONFIG_FAKE_SYSTEMOFF
/*
 * fake_sysoff_status_query
 * return 1: sysoff mode
 * return 0: normal mode
 */
extern unsigned int fake_sysoff_status_query(void);
extern unsigned int fake_sysoff_block_onkey(void);
extern void fake_sysoff_work_cancel(void);
extern void fake_sysoff_set_block_onkey(int block);
#else /*!CONFIG_FAKE_SYSTEMOFF*/
static inline unsigned int fake_sysoff_status_query(void) { return 0; }
static inline unsigned int fake_sysoff_block_onkey(void) { return 0; }
static inline void fake_sysoff_work_cancel(void) {}
static inline void fake_sysoff_set_block_onkey(int block) {}
#endif

#endif

