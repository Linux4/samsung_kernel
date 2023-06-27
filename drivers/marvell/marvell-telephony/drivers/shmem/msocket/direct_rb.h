#ifndef DIAG_PATH_H
#define DIAG_PATH_H

enum direct_rb_type {
	direct_rb_type_diag,
	direct_rb_type_total_cnt
};

struct direct_rbctl {
	int refcount;
	int svc_id;
	enum direct_rb_type direct_type;

	struct shm_rbctl *rbctl;
	bool is_ap_recv_empty;
	bool is_linkdown_msg_received;
	bool is_linkup_msg_received;

	/*
	* wait queue and lock
	*/
	wait_queue_head_t rb_rx_wq;
	spinlock_t rb_rx_lock;
};

extern int direct_rb_init(void);
extern void direct_rb_exit(void);
extern struct direct_rbctl* direct_rb_open(enum direct_rb_type direct_type, int svc_id);
extern void direct_rb_close(struct direct_rbctl *rbctl);
extern int direct_rb_xmit(enum direct_rb_type direct_type, const char __user *buf, int len);
extern ssize_t direct_rb_recv(enum direct_rb_type direct_type, char *buf, int len);
extern void direct_rb_broadcast_msg(int proc);
#endif /* DIAG_PATH_H */
