#ifndef __SPRD_EIRQSOFF_H
#define __SPRD_EIRQSOFF_H

#ifdef CONFIG_SPRD_EIRQSOFF
void start_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
void stop_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
void pause_eirqsoff_timing(void);
void continue_eirqsoff_timing(void);

#ifdef CONFIG_PREEMPT_TRACER
void start_epreempt_timing(unsigned long ip, unsigned long parent_ip);
void stop_epreempt_timing(unsigned long ip, unsigned long parent_ip);
void pause_epreempt_timing(void);
void continue_epreempt_timing(void);
#else /* !CONFIG_PREEMPT_TRACER */
#define start_epreempt_timing(ip, parent_ip)	do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)	do { } while (0)
#define pause_epreempt_timing()			do { } while (0)
#define continue_epreempt_timing()		do { } while (0)
#endif

#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
void start_irqsoff_panic_timing(void);
void stop_irqsoff_panic_timing(void);
#else
#define start_irqsoff_panic_timing()		do { } while (0)
#define stop_irqsoff_panic_timing()		do { } while (0)
#endif

#else /* !CONFIG_SPRD_EIRQSOFF */
#define start_eirqsoff_timing(ip, parent_ip)	do { } while (0)
#define stop_eirqsoff_timing(ip, parent_ip)	do { } while (0)
#define pause_eirqsoff_timing()			do { } while (0)
#define continue_eirqsoff_timing()		do { } while (0)
#define start_epreempt_timing(ip, parent_ip)	do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)	do { } while (0)
#define pause_epreempt_timing()			do { } while (0)
#define continue_epreempt_timing()		do { } while (0)
#define start_irqsoff_panic_timing()		do { } while (0)
#define stop_irqsoff_panic_timing()		do { } while (0)
#endif

#endif
