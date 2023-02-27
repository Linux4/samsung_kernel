/*
 * SPRD CPU USAGE TOOL:
 *    1. cpu usage per real-cpu & total
 *    2. cpu usage per thread
 *    3. cpu system info per real-cpu & total:
 *		contextswitch, pagefault, pagemajfault per cpu & total
 * */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/irqnr.h>
#include <linux/tick.h>
#include <linux/threads.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <asm/cputime.h>
#include <asm/div64.h>
#include <asm/uaccess.h>
#include <linux/sysrq.h>
#ifdef CONFIG_VM_EVENT_COUNTERS
#include <../../../kernel/sched/sched.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#endif

/*
 * Macros Definitions:
 * ----------------------------------------------
 * 1. DEBUG_PRINT : debug log control
 * 2. buffer_size : define ringbuffer size
 * 3. tick_2_ms   : change tick to ms
 * 4. ns_2_ms     : change ns to ms
 * 5. ns_2_us     : change ns to us
 * 6. bufferid    : change index to buffer id
 * */
#define DEBUG_PRINT 0
#define buffer_size 3
#define tick_2_ms(time) (10 * (time))
#define ns_2_ms(time) (do_div(time, 1000000))
#define ns_2_us(time) (do_div(time, 1000))
#define bufferid(index) ((index) % (buffer_size))

/*
 * CpuInfo Structure Per Thread:
 * ----------------------------------------------
 * 1. Located at each-thread stack area, beside threadinfo struct:
 *  HIGH ADDR ---> Thread Stack End.
 *   |--------------------------
 *   |    ...
 *   |    stack
 *   |    ...
 *   |--------------------------
 *   |    thread_cpuinfo
 *   |--------------------------
 *   |    thread_info
 *   |--------------------------
 *  LOW ADDR ---> Thread Stack Start.
 *
 * 2. Cpu Threadinfo:
 *   index         : for update;
 *   start         : utime stime start record;
 *   persistance[] : ringbuffer --> size according to buffer_size;
 * */
typedef struct thread_cpuinfo {
	unsigned long index;
	struct u_s_time {
		unsigned long utime;
		unsigned long stime;
	}start, persistance[buffer_size];
}thread_cpuinfo;

/*
 * Cpu Info Structure:
 * ----------------------------------------------
 *   user    : user usage, percentage;
 *   system  : system usage, percentage;
 *   nice    : nice usage, percentage;
 *   idle    : idle usage, percentage;
 *   iowait  : iowait usage, percentage;
 *   irq     : irq usage, percentage;
 *   softirq : softirq usage, percentage;
 *   steal   : steal usage, percentage;
 *   sum     : sum usage, percentage;
 *   ------------------------------------
 *   ctxt_switch  : context switch count;
 *   pagefault    : page fault count;
 *   pagemajfault : page majfault count;
 * */
typedef struct cpu_info {
	unsigned long long user;
	unsigned long long system;
	unsigned long long nice;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long irq;
	unsigned long long softirq;
	unsigned long long steal;
	unsigned long long sum;
#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long context_switch;
	unsigned long pagefault;
	unsigned long pagemajfault;
#endif
}cpu_info;

/*
 * Time Stamp Structure:
 * ----------------------------------------------
 *  1. start    : this record start cpu clock
 *  2. end      : this record end cpu clock
 *  3. ts_start :
 *  4. ts_end   :
 *  5. tm_start :
 *  6. tm_end   :
 * */
typedef struct time_stamp {
	unsigned long long start;
	unsigned long long end;
	struct timespec ts_start;
	struct timespec ts_end;
	struct rtc_time tm_start;
	struct rtc_time tm_end;
}time_stamp;

/*
 * Recorder Structure:
 * ----------------------------------------------
 * 1. percpu    : each cpu cpuinfo
 * 2. total     : total cpu_info
 * 3. timestamp : timestamp for this record
 * */
typedef struct cpu_recorder {
	struct cpu_info percpu[NR_CPUS];
	struct cpu_info total;
	struct time_stamp timestamp;
}cpu_recorder;

/*
 * Global Data:
 * ----------------------------------------------
 * 1. hrtimer      : for update
 * 2. global_index : for per thread
 * 3. usage_lock   : spinlock for update & print
 * 4. recorder     : for cpu usage
 * */
ktime_t kt;
long hrtimer_se = 3;
long long hrtimer_ns = 0;
static struct hrtimer timer;
static unsigned long global_index = 0;
static DEFINE_SPINLOCK(usage_lock);
static struct cpu_recorder recorder[buffer_size];
static struct cpu_info prev_value[NR_CPUS];
static struct timespec ts;
static unsigned long long each_time = 0;

/*
 * Extern Function:
 * ----------------------------------------------
 * 1. simeple_strtol : change string to long
 * 2. dump_stack     : for debug
 * */
extern long simple_strtol(const char *cp, char **endp, unsigned int base);
extern void dump_stack(void);


/*
 * Functions Start
 * ----------------------------------------------
 * */
static void div64_compute_ratio(const unsigned long long dividend, const unsigned long long divider, unsigned long *result)
{
	/*check divider*/
	if(0 == divider) {
		result[0] = result[1] = 0;
		return;
	}

	/*64bit divition*/
	result[0] = 10000 * dividend;
	result[1] = do_div(result[0], divider);

	/*save result as xx.xx%*/
	result[1] = result[0] % 100;
	result[0] = result[0] / 100;

#if 0
	/*to avoid cpu rate > 100%*/
	if(result[0] > 100) {
		result[0] = 100;
		result[1] = 0;
	}
#endif
}

#ifdef arch_idle_time
static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	/* FIXME: the idle time from get_cpu_idle_time_us() is reset while CPU is hot-pluged.
	 *        Using cpustat[CPUTIME_IDLE] to get idle. It isn't very accurate, but stable */
#if 0
	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);
#endif

	if (idle_time == -1ULL)
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	//FIXME: this idle function has bug on our shark platform:
	//       not monotone increasing
	if(DEBUG_PRINT) printk("acedebug: get_idle_time: cpu=%d, idle=%llu\n", cpu, idle);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	/* FIXME: the iowait time from get_cpu_iowait_time_us() is reset while CPU is hot-pluged.
	 *        Using cpustat[CPUTIME_IOWAIT] to get iowait. It isn't very accurate, but stable */
#if 0
	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);
#endif

	if (iowait_time == -1ULL)
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}
#endif

static void get_each_cpu_data(struct cpu_info *new, int cpuid)
{
#ifdef CONFIG_VM_EVENT_COUNTERS
	struct vm_event_state *this = &per_cpu(vm_event_states, cpuid);
	new->context_switch = cpu_rq(cpuid)->nr_switches;
	new->pagefault = this->event[PGFAULT];
	new->pagemajfault = this->event[PGMAJFAULT];
#endif

	new->sum = new->user = kcpustat_cpu(cpuid).cpustat[CPUTIME_USER];
	new->sum += new->system = kcpustat_cpu(cpuid).cpustat[CPUTIME_SYSTEM];
	new->sum += new->nice = kcpustat_cpu(cpuid).cpustat[CPUTIME_NICE];
	new->sum += new->idle = get_idle_time(cpuid);
	new->sum += new->iowait = get_iowait_time(cpuid);
	new->sum += new->irq = kcpustat_cpu(cpuid).cpustat[CPUTIME_IRQ];
	new->sum += new->softirq = kcpustat_cpu(cpuid).cpustat[CPUTIME_SOFTIRQ];
	new->sum += new->steal = kcpustat_cpu(cpuid).cpustat[CPUTIME_STEAL];
}

static void update_cpu_record(struct cpu_recorder *record, struct cpu_info *prev, struct cpu_info *next, int cpuid)
{
	record->total.user += record->percpu[cpuid].user = next->user - prev->user;
	record->total.system += record->percpu[cpuid].system = next->system - prev->system;
	record->total.nice += record->percpu[cpuid].nice = next->nice - prev->nice;
	/*
	 * FIXME begin: idle from system maybe wrong!!!
	 *      in our multi-core system idle may not monotone increasing!!!
	 *      SO: here we walk-around, but need modify in future!!!
	 */
	if(next->idle < prev->idle) {
		prev->idle = (next->idle > 200) ? (next->idle - 200) : 0;
		prev->sum = prev->user + prev->system + prev->nice + prev->idle + prev->iowait + prev->irq + prev->softirq + prev->steal;
	}

	record->total.idle += record->percpu[cpuid].idle = next->idle - prev->idle;
	record->total.iowait += record->percpu[cpuid].iowait = next->iowait - prev->iowait;
	record->total.irq += record->percpu[cpuid].irq = next->irq - prev->irq;
	record->total.softirq += record->percpu[cpuid].softirq = next->softirq - prev->softirq;
	record->total.steal += record->percpu[cpuid].steal = next->steal - prev->steal;
	record->total.sum += record->percpu[cpuid].sum = next->sum - prev->sum;

#ifdef CONFIG_VM_EVENT_COUNTERS
	/*
	 * FIXME begin: pagefault & pagemajfault maybe wrong!!!
	 *       in our multi-core system pagefault & pagemajfault may not monotone increasing!!!
	 *       SO: here we walk-around, but need modify in future!!!
	 */
	if(next->context_switch < prev->context_switch) prev->context_switch = next->context_switch;
	if(next->pagefault < prev->pagefault) prev->pagefault = next->pagefault;
	if(next->pagemajfault < prev->pagemajfault) prev->pagemajfault = next->pagemajfault;

	record->total.context_switch += record->percpu[cpuid].context_switch = next->context_switch - prev->context_switch;
	record->total.pagefault += record->percpu[cpuid].pagefault = next->pagefault - prev->pagefault;
	record->total.pagemajfault += record->percpu[cpuid].pagemajfault = next->pagemajfault - prev->pagemajfault;
#endif
}

static void cpu_threadinfo_clean(struct thread_cpuinfo *buffer)
{
	unsigned long i = 0;
	unsigned long begin = 0;
	unsigned long end = 0;

	begin = bufferid(buffer->index) + 1;
	end = bufferid(global_index);
	if((global_index - buffer->index) >= buffer_size) {
		begin = 0;
		end = buffer_size - 1;
	}

	if(begin < end + 1) {
		for(i = begin; i < end + 1; i++) {
			buffer->persistance[i].utime = 0;
			buffer->persistance[i].stime = 0;
		}
	} else {
		for(i = begin; i < buffer_size; i++) {
			buffer->persistance[i].utime = 0;
			buffer->persistance[i].stime = 0;
		}
		for(i = 0; i < end + 1; i++) {
			buffer->persistance[i].utime = 0;
			buffer->persistance[i].stime = 0;
		}
	}
}

static void update_prev(struct task_struct *prev)
{
	struct thread_cpuinfo *buffer = NULL;

	/*ignore pid=0 process*/
	//if(!strncmp(prev->comm, "swapper/", 8)) return;

	/*thread buffer*/
	buffer = (struct thread_cpuinfo *)(prev->stack + sizeof(struct thread_info));
	if(!buffer) return;

	/*update*/
	if(buffer->index == global_index) {
		/*update utime stime*/
		buffer->persistance[bufferid(buffer->index)].utime += prev->utime - buffer->start.utime;
		buffer->persistance[bufferid(buffer->index)].stime += prev->stime - buffer->start.stime;
	} else {
		/*clean buffer*/
		cpu_threadinfo_clean(buffer);

		/*update index utime stime*/
		buffer->index = global_index;
		buffer->persistance[bufferid(buffer->index)].utime = prev->utime - buffer->start.utime;
		buffer->persistance[bufferid(buffer->index)].stime = prev->stime - buffer->start.stime;
	}
}

static void update_next(struct task_struct *next)
{
	struct thread_cpuinfo *buffer = NULL;

	/*ignore pid=0 process*/
	//if(!strncmp(next->comm, "swapper/", 8)) return;

	/*thread buffer*/
	buffer = (struct thread_cpuinfo *)(next->stack + sizeof(struct thread_info));
	if(!buffer) return;

	/*update utime stime*/
	buffer->start.utime = next->utime;
	buffer->start.stime = next->stime;
}


void update_cpu_usage(struct task_struct *prev, struct task_struct *next)
{
	unsigned long flags;

	/*lock*/
	spin_lock_irqsave(&usage_lock, flags);

	/*prev*/
	update_prev(prev);

	/*next*/
	update_next(next);

	/*unlock*/
	spin_unlock_irqrestore(&usage_lock, flags);

	return;
}

static void record_cpu_usage(void)
{
	int i, pos;
	struct cpu_info next;

	/*init*/
	pos = bufferid(global_index);
	memset(&next, 0, sizeof(struct cpu_info));
	memset(&recorder[pos], 0, sizeof(struct cpu_recorder));

	/*record start time*/
	recorder[pos].timestamp.start = each_time;
	memcpy(&recorder[pos].timestamp.ts_start, &ts, sizeof(struct timespec));
	rtc_time_to_tm(recorder[pos].timestamp.ts_start.tv_sec, &recorder[pos].timestamp.tm_start);

	/*record end time*/
	recorder[pos].timestamp.end = each_time = cpu_clock(0);
	getnstimeofday(&ts);
	memcpy(&recorder[pos].timestamp.ts_end, &ts, sizeof(struct timespec));
	rtc_time_to_tm(recorder[pos].timestamp.ts_end.tv_sec, &recorder[pos].timestamp.tm_end);

	/*update recorder[pos] cpu data*/
	for_each_possible_cpu(i) {
		/*get next*/
		get_each_cpu_data(&next, i);

		/*update recorder*/
		update_cpu_record(&recorder[pos], &prev_value[i], &next, i);

		/*update prev*/
		memcpy(&prev_value[i], &next, sizeof(struct cpu_info));
	}

	/*update global index*/
	global_index++;
}

static void compute_print_cpu_rate(struct seq_file *p, void *v, struct cpu_recorder *record)
{
	unsigned long idle_ratio[2] = {0, 0};
	unsigned long user_ratio[2] = {0, 0};
	unsigned long system_ratio[2] = {0, 0};
	unsigned long nice_ratio[2] = {0, 0};
	unsigned long iowait_ratio[2] = {0, 0};
	unsigned long irq_ratio[2] = {0, 0};
	unsigned long softirq_ratio[2] = {0, 0};
	unsigned long steal_ratio[2] = {0, 0};
	unsigned long sum_ratio[2] = {0, 0};
	int i;

	for_each_possible_cpu(i) {
		/*compute each cpu*/
		div64_compute_ratio(record->percpu[i].idle, record->percpu[i].sum, idle_ratio);
		div64_compute_ratio(record->percpu[i].user, record->percpu[i].sum, user_ratio);
		div64_compute_ratio(record->percpu[i].system, record->percpu[i].sum, system_ratio);
		div64_compute_ratio(record->percpu[i].nice, record->percpu[i].sum, nice_ratio);
		div64_compute_ratio(record->percpu[i].iowait, record->percpu[i].sum, iowait_ratio);
		div64_compute_ratio(record->percpu[i].irq, record->percpu[i].sum, irq_ratio);
		div64_compute_ratio(record->percpu[i].softirq, record->percpu[i].sum, softirq_ratio);
		div64_compute_ratio(record->percpu[i].steal, record->percpu[i].sum, steal_ratio);
		div64_compute_ratio(record->percpu[i].sum, record->percpu[i].sum, sum_ratio);
		/*print each cpu*/
		seq_printf(p, " cpu%d(%d): %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15lu %15lu %15lu\n",
				i, cpu_online(i), idle_ratio[0],idle_ratio[1], user_ratio[0],user_ratio[1], system_ratio[0],system_ratio[1], \
				nice_ratio[0],nice_ratio[1], iowait_ratio[0],iowait_ratio[1], irq_ratio[0],irq_ratio[1], \
				softirq_ratio[0],softirq_ratio[1], steal_ratio[0],steal_ratio[1], sum_ratio[0],sum_ratio[1], \
				record->percpu[i].context_switch, record->percpu[i].pagefault, record->percpu[i].pagemajfault);
	}

	if(NR_CPUS > 1) {
		/*compute total*/
		div64_compute_ratio(record->total.idle, record->total.sum, idle_ratio);
		div64_compute_ratio(record->total.user, record->total.sum, user_ratio);
		div64_compute_ratio(record->total.system, record->total.sum, system_ratio);
		div64_compute_ratio(record->total.nice, record->total.sum, nice_ratio);
		div64_compute_ratio(record->total.iowait, record->total.sum, iowait_ratio);
		div64_compute_ratio(record->total.irq, record->total.sum, irq_ratio);
		div64_compute_ratio(record->total.softirq, record->total.sum, softirq_ratio);
		div64_compute_ratio(record->total.steal, record->total.sum, steal_ratio);
		div64_compute_ratio(record->total.sum, record->total.sum, sum_ratio);
		/*print total*/
		seq_printf(p, " ------------------\n");
		seq_printf(p, " Total:   %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15lu %15lu %15lu\n",
				idle_ratio[0],idle_ratio[1], user_ratio[0],user_ratio[1], system_ratio[0],system_ratio[1], \
				nice_ratio[0],nice_ratio[1], iowait_ratio[0],iowait_ratio[1], irq_ratio[0],irq_ratio[1], \
				softirq_ratio[0],softirq_ratio[1], steal_ratio[0],steal_ratio[1], sum_ratio[0],sum_ratio[1], \
				record->total.context_switch, record->total.pagefault, record->total.pagemajfault);
	}
}

static void compute_print_each_thread_rate(struct seq_file *p, void *v, int pid, char *name,
	unsigned long u_time, unsigned long s_time, unsigned long index)
{
	unsigned long utime_ratio[2] = {0, 0};
	unsigned long stime_ratio[2] = {0, 0};
	unsigned long ttime_ratio[2] = {0, 0};
	unsigned long timer_circle_long = 0;

	/*devider*/
	timer_circle_long = recorder[index].timestamp.end - recorder[index].timestamp.start;
	ns_2_ms(timer_circle_long);

	/*compute ratio*/
	div64_compute_ratio(u_time, timer_circle_long, utime_ratio);
	div64_compute_ratio(s_time, timer_circle_long, stime_ratio);
	div64_compute_ratio((u_time + s_time), timer_circle_long, ttime_ratio);

	if(NULL != name) {
		seq_printf(p, " %-6d  %4lu.%02lu%%  %4lu.%02lu%%  %4lu.%02lu%%    %-15s\n",
				pid, utime_ratio[0],utime_ratio[1], stime_ratio[0],stime_ratio[1], ttime_ratio[0],ttime_ratio[1], name);
	} else {
		seq_printf(p, " %-6s  %4lu.%02lu%%  %4lu.%02lu%%  %4lu.%02lu%%\n",
				 "Total:", utime_ratio[0],utime_ratio[1], stime_ratio[0],stime_ratio[1], ttime_ratio[0],ttime_ratio[1]);
	}
}

static void print_summary(struct seq_file *p, void *v, unsigned long bufid)
{
	unsigned long long timelongth;
	unsigned long long start_time = 0;
	unsigned long long end_time = 0;

	/*update recorder[bufid].timestamp temporarily*/
	if(bufid == bufferid(global_index)) {
		recorder[bufid].timestamp.start = each_time;
		recorder[bufid].timestamp.end = cpu_clock(0);
		memcpy(&recorder[bufid].timestamp.ts_start, &ts, sizeof(struct timespec));
		getnstimeofday(&recorder[bufid].timestamp.ts_end);
		rtc_time_to_tm(recorder[bufid].timestamp.ts_start.tv_sec, &recorder[bufid].timestamp.tm_start);
		rtc_time_to_tm(recorder[bufid].timestamp.ts_end.tv_sec, &recorder[bufid].timestamp.tm_end);
	}

	/*compute how long time*/
	timelongth = recorder[bufid].timestamp.end - recorder[bufid].timestamp.start;

	/*start_time & end_time*/
	start_time = recorder[bufid].timestamp.start;
	end_time = recorder[bufid].timestamp.end;

	/*change ns to ms*/
	ns_2_ms(start_time);
	ns_2_ms(end_time);
	ns_2_ms(timelongth);

	/*print cpu core count*/
	seq_printf(p, "\n\nCpu Core Count: %-6d\n", NR_CPUS);

	/*from start_time to end_time*/
	seq_printf(p, "Timer Circle: %-llums.\n",timelongth);
	seq_printf(p, "  From time %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC) to %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC).\n\n",
			start_time, recorder[bufid].timestamp.tm_start.tm_year + 1900, recorder[bufid].timestamp.tm_start.tm_mon + 1,
			recorder[bufid].timestamp.tm_start.tm_mday, recorder[bufid].timestamp.tm_start.tm_hour, recorder[bufid].timestamp.tm_start.tm_min,
			recorder[bufid].timestamp.tm_start.tm_sec, recorder[bufid].timestamp.ts_start.tv_nsec,
			end_time, recorder[bufid].timestamp.tm_end.tm_year + 1900, recorder[bufid].timestamp.tm_end.tm_mon + 1,
			recorder[bufid].timestamp.tm_end.tm_mday, recorder[bufid].timestamp.tm_end.tm_hour, recorder[bufid].timestamp.tm_end.tm_min,
			recorder[bufid].timestamp.tm_end.tm_sec, recorder[bufid].timestamp.ts_end.tv_nsec);
}

static void print_cpu_usage(struct seq_file *p, void *v, unsigned long bufid)
{
	int i;
	struct cpu_info now;

	/*init*/
	memset(&now, 0, sizeof(cpu_info));

	/*update current record temporarily*/
	if(bufid == bufferid(global_index)) {
		memset(&recorder[bufid].percpu, 0, NR_CPUS * sizeof(struct cpu_info));
		memset(&recorder[bufid].total, 0, sizeof(struct cpu_info));

		/*data*/
		for_each_possible_cpu(i) {
			/*get now*/
			get_each_cpu_data(&now, i);

			/*update recorder*/
			update_cpu_record(&recorder[bufid], &prev_value[i], &now, i);
		}
	}

	/*print tile*/
	seq_printf(p, "%-87s   %-s\n", " * CPU USAGE:", " | * OTHER COUNTS:");
	seq_printf(p, " -%lu-      %8s %8s %8s %8s %8s %8s %8s %8s %8s | %15s %15s %15s\n",
			bufid, "IDLE", "USER", "SYSTEM", "NICE", "IOWAIT", "IRQ", "SOFTIRQ", "STEAL", "TOTAL", "CTXT_SWITCH", "FG_FAULT", "FG_MAJ_FAULT");

	/*compute & print*/
	compute_print_cpu_rate(p, v, &recorder[bufid]);
}

static void print_each_thread_usage(struct seq_file *p, void *v, unsigned long bufid)
{
	unsigned long total_usr = 0;
	unsigned long total_sys = 0;
	struct task_struct *gp, *pp;
	struct thread_cpuinfo *buffer = NULL;

	/*print tile*/
	seq_printf(p, "\n* USAGE PER THREAD:\n");
	seq_printf(p, " %-6s  %8s  %8s  %8s    %-15s\n", "PID", "USER", "SYSTEM", "TOTAL", "NAME");

	/*for all threads*/
	do_each_thread(gp, pp) {

		/*get ringbuffer*/
		buffer = (struct thread_cpuinfo *)(pp->stack + sizeof(struct thread_info));
		if(!buffer) continue;

		/*clean ringbuffer*/
		if(global_index != buffer->index) {

			cpu_threadinfo_clean(buffer);

			/*update index*/
			buffer->index = global_index;
		}

		/*print each none zero thread*/
		if(0 != (buffer->persistance[bufid].utime + buffer->persistance[bufid].stime)) {

			unsigned long u_time = tick_2_ms(buffer->persistance[bufid].utime);
			unsigned long s_time = tick_2_ms(buffer->persistance[bufid].stime);

			/*compute total*/
			total_usr += buffer->persistance[bufid].utime;
			total_sys += buffer->persistance[bufid].stime;

			compute_print_each_thread_rate(p, v, pp->pid, pp->comm, u_time, s_time, bufid);
		}
	} while_each_thread(gp, pp);

	/*print total*/
	seq_printf(p, " ------------------\n");
	compute_print_each_thread_rate(p, v, 0, NULL, (tick_2_ms(total_usr)), (tick_2_ms(total_sys)), bufid);
}

static void print_usage(struct seq_file *p, void *v)
{
	unsigned long flags;
	unsigned long i;
        /*trigger sysrq when print usage for extra debug info*/
        __handle_sysrq('w' , false);
	/*lock*/
	spin_lock_irqsave(&usage_lock, flags);

	/*each thread: global_index+1 ---> buffer_end*/
	for(i = bufferid(global_index) + 1; i < buffer_size; i++) {

		/*print time circle*/
		print_summary(p, v, i);

		/*print cpu info*/
		print_cpu_usage(p, v, i);

		/*print thread cpu_info*/
		print_each_thread_usage(p, v, i);
	}

	/*each thread: buffer_start ---> global_index */
	for(i = 0; i < bufferid(global_index) + 1; i++) {

		/*print time circle*/
		print_summary(p, v, i);

		/*print cpu info*/
		print_cpu_usage(p, v, i);

		/*print thread cpu_info*/
		print_each_thread_usage(p, v, i);
	}

	/*unlock*/
	spin_unlock_irqrestore(&usage_lock, flags);
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{
	unsigned long flags;

	/*lock*/
	spin_lock_irqsave(&usage_lock, flags);

	/*update cpu rate*/
	record_cpu_usage();

	/*update hrtimer*/
	if((hrtimer_se > 0) || (hrtimer_ns >0)) {
		kt = ktime_set(hrtimer_se, hrtimer_ns);
	}

	hrtimer_forward_now(timer, kt);

	/*unlock*/
	spin_unlock_irqrestore(&usage_lock, flags);

	return HRTIMER_RESTART;
}

static int show_cpu_usage(struct seq_file *p, void *v)
{
	print_usage(p, v);

	return 0;
}

static int cpu_usage_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char flag[len + 1];
	char **endp = NULL;

	memset(flag, 0 ,sizeof(flag));

	if(copy_from_user(flag, buf, len)) return -EFAULT;

	hrtimer_se = simple_strtol(flag, endp, 10);

	return len;
}

static int cpu_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_cpu_usage, NULL);
}

struct file_operations proc_cpu_usage_fops = {
	.open		= cpu_usage_open,
	.read		= seq_read,
	.write		= cpu_usage_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_sprd_cpu_usage_init(void)
{
#ifdef CONFIG_SPRD_CPU_RATE
	/*int static*/
	memset(recorder, 0, sizeof(recorder));
	memset(prev_value, 0, sizeof(prev_value));
	memset(&ts, 0, sizeof(struct timespec));

	/*init timer*/
	kt = ktime_set(hrtimer_se, hrtimer_ns);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = hrtimer_handler;
	hrtimer_start(&timer, kt, HRTIMER_MODE_REL);

	/*create proc*/
	//proc_create("sprd_cpu_usage", 0, NULL, &proc_cpu_usage_fops);
#endif

	return 0;
}

static void __exit proc_sprd_cpu_usage_exit(void)
{
#ifdef CONFIG_SPRD_CPU_RATE
	hrtimer_cancel(&timer);
#endif
	return;
}

module_init(proc_sprd_cpu_usage_init);
module_exit(proc_sprd_cpu_usage_exit);

