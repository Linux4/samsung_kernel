#define mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data, irq_type) \
kunit_mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data, irq_type)

#define mif_stream_peek(stream, current_packet) \
kunit_mif_stream_peek(stream, current_packet)

#define mif_stream_peek_complete(stream, packet) \
kunit_mif_stream_peek_complete(stream, packet)

static void input_irq_handler(int irq, void *data);
void (*fp_mxmgmt_input_irq_handler)(int irq, void *data) = &input_irq_handler;

static void input_irq_handler_wpan(int irq, void *data);
void (*fp_mxmgmt_input_irq_handler_wpan)(int irq, void *data) = &input_irq_handler_wpan;

static void output_irq_handler(int irq, void *data);
void (*fp_mxmgmt_output_irq_handler)(int irq, void *data) = &output_irq_handler;

static void output_irq_handler_wpan(int irq, void *data);
void (*fp_mxmgmt_output_irq_handler_wpan)(int irq, void *data) = &output_irq_handler_wpan;

int peek_stream_repeat = 1;
static struct mxmgr_message current_message;
void *kunit_mif_stream_peek(struct mif_stream *stream, const void *current_packet)
{
	if (peek_stream_repeat-- > 0) {
		return &current_message;
	}
	return NULL;

}
void kunit_mif_stream_peek_complete(struct mif_stream *stream, const void *packet)
{
}
