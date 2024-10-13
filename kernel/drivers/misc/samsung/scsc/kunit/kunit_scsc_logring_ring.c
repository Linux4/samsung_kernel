#define wake_up_interruptible(args...)			((void *)0)

static inline uint32_t get_calculated_crc(struct scsc_ring_record *rec, loff_t pos);
uint32_t (*fp_get_calculated_crc)(struct scsc_ring_record *rec, loff_t pos) = &get_calculated_crc;

static inline bool is_ring_read_pos_valid(struct scsc_ring_buffer *rb, loff_t pos);
bool (*fp_is_ring_read_pos_valid)(struct scsc_ring_buffer *rb, loff_t pos) = &is_ring_read_pos_valid;

static inline void scsc_ring_buffer_overlap_append(struct scsc_ring_buffer *rb, const char *srcbuf, int slen, const char *hbuf, int hlen);
void (*fp_scsc_ring_buffer_overlap_append)(struct scsc_ring_buffer *rb, const char *srcbuf, int slen, const char *hbuf, int hlen) = &scsc_ring_buffer_overlap_append;

static inline int binary_hexdump(char *tbuf, int tsz, struct scsc_ring_record *rrec, int start, int dlen);
int (*fp_binary_hexdump)(char *tbuf, int tsz, struct scsc_ring_record *rrec, int start, int dlen) = &binary_hexdump;

static inline size_t tag_reader_binary(char *tbuf, struct scsc_ring_buffer *rb, int start_rec, size_t tsz);
size_t (*fp_tag_reader_binary)(char *tbuf, struct scsc_ring_buffer *rb, int start_rec, size_t tsz) = &tag_reader_binary;

static inline loff_t reader_resync(struct scsc_ring_buffer *rb, loff_t invalid_pos, int *resynced_bytes);
loff_t (*fp_reader_resync)(struct scsc_ring_buffer *rb, loff_t invalid_pos, int *resynced_bytes) = &reader_resync;
