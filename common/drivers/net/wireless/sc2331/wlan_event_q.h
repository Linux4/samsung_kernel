#ifndef __EVENT_MANAGER_H__
#define __EVENT_MANAGER_H__
#include <linux/spinlock.h>

#define WORD_ALIGN(val) (((val) & 0x03) ? ((val) + 4 - ((val) & 0x03)) : (val))

#define MAX_WORD_ALIGNMENT_BUFFER   (3)
#define EVENT_TOTAL_MEM_SIZE(num, size)     ((WORD_ALIGN((num >> 2) + 1))  +   ((num)*(size))   +  (MAX_WORD_ALIGNMENT_BUFFER))
#define EVENT_STATUS_SIZE(num)                           (WORD_ALIGN((num >> 2) + 1))

typedef enum {
	EVENT_BUFFER_FREE = 0,
	EVENT_BUFFER_ALLOC = 1,
	EVENT_BUFFER_VALID = 2,
} EVENT_BUFFER_STATUS_T;

typedef struct {
	unsigned char *event_buf;	/* Base address of event buffer array               */
	unsigned char *buf_status;	/* Base address of buffer status array              */
	unsigned short event_size;	/* Size of the event message structure              */
	unsigned short max_events;	/* Number of events can be accomadated in the queue */
	unsigned short event_cnt;	/* Current number of valid events                   */
	unsigned short tail_index;	/* Last index inserted                              */
	unsigned short head_index;	/* Next index to be read                            */
	unsigned short highThres;
	unsigned short lowThres;
	unsigned short weight;
	spinlock_t spinlock;
} m_event_t;

typedef struct {
	unsigned short event_size;
	unsigned short max_events;
	unsigned short highThres;
	unsigned short lowThres;
	unsigned short weight;
} m_event_conf_t;

extern void  *alloc_event(m_event_t *mEventQ);
extern void post_event(unsigned char *event, m_event_t *mEventQ);
extern unsigned char *get_event(m_event_t *mEventQ);
extern void free_event(void *event, m_event_t *mEventQ);
extern int event_q_init(m_event_t *mEventQ, m_event_conf_t *conf);
extern int event_q_deinit(m_event_t *mEventQ);
#endif

