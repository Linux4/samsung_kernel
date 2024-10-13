#define mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data, irq_type) \
kunit_mif_stream_init(stream, target, direction, num_packets, packet_size, mx, intrbit, tohost_irq_handler, data, irq_type)

#define mif_stream_write(stream, buf, num_bytes) \
kunit_gdb_mif_stream_write(stream, buf, num_bytes)

#define mif_stream_read(stream, buf, num_bytes) \
kunit_gdb_mif_stream_read(stream, buf, num_bytes)

static void gdb_input_irq_handler(int irq, void *data);
void (*fp_gdb_input_irq_handler)(int irq, void *data) = &gdb_input_irq_handler;

static void gdb_output_irq_handler(int irq, void *data);
void (*fp_gdb_output_irq_handler)(int irq, void *data) = &gdb_output_irq_handler;

static int read_stream_count = 0;
uint32_t kunit_gdb_mif_stream_read(struct mif_stream *stream, void *buf, uint32_t num_bytes)
{
	if (read_stream_count == 0) {
		*(uint8_t *)buf = 1;
		read_stream_count = 1;
		return 1;
	} else if (read_stream_count == 1) {
		read_stream_count = 2;
		return 1;
	} else if (read_stream_count == 2) {
		read_stream_count = 0;
		return 0;
	}
	return 0;
}

static int write_stream_count = 0;
uint32_t kunit_gdb_mif_stream_write(struct mif_stream *stream, void *buf, uint32_t num_bytes)
{
	if (write_stream_count == 0) {
		write_stream_count = 1;
		return 1;
	} else if (write_stream_count == 1) {
		write_stream_count = 0;
		return 1;
	}
	return 0;
}
