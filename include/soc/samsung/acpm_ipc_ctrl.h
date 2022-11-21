#ifndef __ACPM_IPC_CTRL_H__
#define __ACPM_IPC_CTRL_H__

typedef void (*ipc_callback)(unsigned int *cmd, unsigned int size);

struct ipc_config {
	unsigned int *cmd;
	unsigned int *indirection;
	unsigned int indirection_size;
	bool responce;
};

#define ACPM_IPC_PROTOCOL_OWN			(31)
#define ACPM_IPC_PROTOCOL_RSP			(30)
#define ACPM_IPC_PROTOCOL_INDIRECTION		(29)
#define ACPM_IPC_PROTOCOL_ID			(26)
#define ACPM_IPC_PROTOCOL_IDX			(0x7 << ACPM_IPC_PROTOCOL_ID)
#define ACPM_IPC_PROTOCOL_DP_A			(25)
#define ACPM_IPC_PROTOCOL_DP_D			(24)
#define ACPM_IPC_PROTOCOL_DP_CMD		(0x3 << ACPM_IPC_PROTOCOL_DP_D)
#define ACPM_IPC_PROTOCOL_TEST			(23)
#define ACPM_IPC_PROTOCOL_STOP			(22)

#ifdef CONFIG_EXYNOS_ACPM
unsigned int acpm_ipc_request_channel(struct device_node *np, ipc_callback handler,
		unsigned int *id, unsigned int *size);
unsigned int acpm_ipc_release_channel(unsigned int channel_id);
int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg);

#else

inline unsigned int acpm_ipc_request_channel(struct device_node *np, ipc_callback handler,
		unsigned int *id, unsigned int *size, bool polling)
{
	return 0;
}

inline unsigned int acpm_ipc_release_channel(unsigned int channel_id)
{
	return 0;
}
inline int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg)
{
	return 0;
}
#endif

#endif
