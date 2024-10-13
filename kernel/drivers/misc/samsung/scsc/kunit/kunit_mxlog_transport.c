#include <kunit/test.h>

#define mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data, irq_type) \
kunit_mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data,  irq_type)

#define mif_stream_read(stream, buf, num_bytes) \
kunit_mif_stream_read(stream, buf, num_bytes)

char *read_msg = NULL;

int kunit_mif_stream_init(struct mif_stream *stream, enum scsc_mif_abs_target target, enum MIF_STREAM_DIRECTION direction, uint32_t num_packets, uint32_t packet_size,
		    struct scsc_mx *mx, enum MIF_STREAM_INTRBIT_TYPE intrbit, mifintrbit_handler tohost_irq_handler, void *data, enum IRQ_TYPE irq_type)
{
	return 0;
}

static void input_irq_handler(int irq, void *data);
void (*fp_input_irq_handler)(int irq, void *data) = &input_irq_handler;

static void mxlog_input_irq_handler_wpan(int irq, void *data);
void (*fp_mxlog_input_irq_handler_wpan)(int irq, void *data) = &mxlog_input_irq_handler_wpan;

static void mxlog_thread_stop(struct mxlog_transport *mxlog_transport);
void (*fp_mxlog_thread_stop)(struct mxlog_transport *mxlog_transport) = &mxlog_thread_stop;

int read_stream_repeat = 2;
uint32_t kunit_mif_stream_read(struct mif_stream *stream, void *buf, uint32_t num_bytes)
{
	if (read_stream_repeat-- > 0)
	{
		return 1;
	}
	read_stream_repeat =2;
	return 0;
}
