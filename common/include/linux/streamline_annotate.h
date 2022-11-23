#ifndef STREAMLINE_ANNOTATE_H
#define STREAMLINE_ANNOTATE_H

/*
 *  User-space only macros:
 *  ANNOTATE_DEFINE  You must put 'ANNOTATE_DEFINE;' one place in your program
 *  ANNOTATE_SETUP   Execute at the start of the program before other ANNOTATE macros are called
 *  
 *  User-space and Kernel-space macros:
 *  ANNOTATE(str)                                String annotation
 *  ANNOTATE_CHANNEL(channel, str)               String annotation on a channel
 *  ANNOTATE_COLOR(color, str)                   String annotation with color
 *  ANNOTATE_CHANNEL_COLOR(channel, color, str)  String annotation on a channel with color
 *  ANNOTATE_END()                               Terminate an annotation
 *  ANNOTATE_CHANNEL_END(channel)                Terminate an annotation on a channel
 *  ANNOTATE_NAME_CHANNEL(channel, group, str)   Name a channel and link it to a group
 *  ANNOTATE_NAME_GROUP(group, str)              Name a group
 *  ANNOTATE_VISUAL(data, length, str)           Image annotation with optional string
 *  ANNOTATE_MARKER()                            Marker annotation
 *  ANNOTATE_MARKER_STR(str)                     Marker annotation with a string
 *  ANNOTATE_MARKER_COLOR(color)                 Marker annotation with a color
 *  ANNOTATE_MARKER_COLOR_STR(color, str)        Marker annotation with a string and color
 *
 *  Channels and groups are defined per thread. This means that if the same
 *  channel number is used on different threads they are in fact separate
 *  channels. A channel can belong to only one group per thread. This means
 *  channel 1 cannot be part of both group 1 and group 2 on the same thread.
 *
 *  NOTE: Kernel annotations are not supported in interrupt context.
 *  NOTE: When using threads, ensure you include the -pthread option when both
 *        compiling and linking. Using -lpthread when linking is not sufficient.
 */

/* ESC character, hex RGB (little endian) */
#define ANNOTATE_RED    0x0000ff1b
#define ANNOTATE_BLUE   0xff00001b
#define ANNOTATE_GREEN  0x00ff001b
#define ANNOTATE_PURPLE 0xff00ff1b
#define ANNOTATE_YELLOW 0x00ffff1b
#define ANNOTATE_CYAN   0xffff001b
#define ANNOTATE_WHITE  0xffffff1b
#define ANNOTATE_LTGRAY 0xbbbbbb1b
#define ANNOTATE_DKGRAY 0x5555551b
#define ANNOTATE_BLACK  0x0000001b

#ifdef __KERNEL__  /* Start of kernel-space macro definitions */

#include <linux/module.h>

void gator_annotate(const char* str);
void gator_annotate_channel(int channel, const char* str);
void gator_annotate_color(int color, const char* str);
void gator_annotate_channel_color(int channel, int color, const char* str);
void gator_annotate_end(void);
void gator_annotate_channel_end(int channel);
void gator_annotate_name_channel(int channel, int group, const char* str);
void gator_annotate_name_group(int group, const char* str);
void gator_annotate_visual(const char* data, unsigned int length, const char* str);
void gator_annotate_marker(void);
void gator_annotate_marker_str(const char* str);
void gator_annotate_marker_color(int color);
void gator_annotate_marker_color_str(int color, const char* str);

#define ANNOTATE_INVOKE(func, args) \
	func##_ptr = symbol_get(gator_##func); \
	if (func##_ptr) { \
		func##_ptr args; \
		symbol_put(gator_##func); \
	} \

#define ANNOTATE(str) do { \
	void (*annotate_ptr)(const char*); \
	ANNOTATE_INVOKE(annotate, (str)); \
	} while(0)

#define ANNOTATE_CHANNEL(channel, str) do { \
	void (*annotate_channel_ptr)(int, const char*); \
	ANNOTATE_INVOKE(annotate_channel, (channel, str)); \
	} while(0)

#define ANNOTATE_COLOR(color, str) do { \
	void (*annotate_color_ptr)(int, const char*); \
	ANNOTATE_INVOKE(annotate_color, (color, str)); \
	} while(0)

#define ANNOTATE_CHANNEL_COLOR(channel, color, str) do { \
	void (*annotate_channel_color_ptr)(int, int, const char*); \
	ANNOTATE_INVOKE(annotate_channel_color, (channel, color, str)); \
	} while(0)

#define ANNOTATE_END() do { \
	void (*annotate_end_ptr)(void); \
	ANNOTATE_INVOKE(annotate_end, ()); \
	} while(0)

#define ANNOTATE_CHANNEL_END(channel) do { \
	void (*annotate_channel_end_ptr)(int); \
	ANNOTATE_INVOKE(annotate_channel_end, (channel)); \
	} while(0)

#define ANNOTATE_NAME_CHANNEL(channel, group, str) do { \
	void (*annotate_name_channel_ptr)(int, int, const char*); \
	ANNOTATE_INVOKE(annotate_name_channel, (channel, group, str)); \
	} while(0)

#define ANNOTATE_NAME_GROUP(group, str) do { \
	void (*annotate_name_group_ptr)(int, const char*); \
	ANNOTATE_INVOKE(annotate_name_group, (group, str)); \
	} while(0)

#define ANNOTATE_VISUAL(data, length, str) do { \
	void (*annotate_visual_ptr)(const char*, unsigned int, const char*); \
	ANNOTATE_INVOKE(annotate_visual, (data, length, str)); \
	} while(0)

#define ANNOTATE_MARKER() do { \
	void (*annotate_marker_ptr)(void); \
	ANNOTATE_INVOKE(annotate_marker, ()); \
	} while(0)

#define ANNOTATE_MARKER_STR(str) do { \
	void (*annotate_marker_str_ptr)(const char*); \
	ANNOTATE_INVOKE(annotate_marker_str, (str)); \
	} while(0)

#define ANNOTATE_MARKER_COLOR(color) do { \
	void (*annotate_marker_color_ptr)(int); \
	ANNOTATE_INVOKE(annotate_marker_color, (color)); \
	} while(0)

#define ANNOTATE_MARKER_COLOR_STR(color, str) do { \
	void (*annotate_marker_color_str_ptr)(int, const char*); \
	ANNOTATE_INVOKE(annotate_marker_color_str, (color, str)); \
	} while(0)

#else  /* Start of user-space macro definitions */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _REENTRANT

#include <pthread.h>

extern pthread_mutex_t gator_mutex;

#define ANNOTATE_MUTEX_DEFINE pthread_mutex_t gator_mutex = PTHREAD_MUTEX_INITIALIZER;
#define ANNOTATE_LOCK pthread_mutex_lock(&gator_mutex)
#define ANNOTATE_UNLOCK pthread_mutex_unlock(&gator_mutex)

#else

#define ANNOTATE_MUTEX_DEFINE
#define ANNOTATE_LOCK ((void)0)
#define ANNOTATE_UNLOCK ((void)0)

#endif

extern FILE *gator_annotate;

#define ANNOTATE_DEFINE \
	ANNOTATE_MUTEX_DEFINE \
	FILE *gator_annotate = 0

#define ANNOTATE_SETUP do { if (!gator_annotate) { \
	gator_annotate = fopen("/dev/gator/annotate", "wb"); \
	}} while(0)

#define ANNOTATE(str) ANNOTATE_CHANNEL(0, str)

#define ANNOTATE_CHANNEL(channel, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = strlen(gator_str) & 0xffff; \
	char gator_header[8]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x06; \
	ANNOTATE_MARSHAL_UINT32(gator_header + 2, channel); \
	ANNOTATE_MARSHAL_UINT16(gator_header + 6, gator_str_size); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_COLOR(color, str) ANNOTATE_CHANNEL_COLOR(0, color, str)

#define ANNOTATE_CHANNEL_COLOR(channel, color, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = (strlen(gator_str) + 4) & 0xffff; \
	char gator_header[12]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x06; \
	ANNOTATE_MARSHAL_UINT32(gator_header + 2, channel); \
	ANNOTATE_MARSHAL_UINT16(gator_header + 6, gator_str_size); \
	ANNOTATE_MARSHAL_UINT32(gator_header + 8, color); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size - 4); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_END() ANNOTATE_CHANNEL_END(0)

#define ANNOTATE_CHANNEL_END(channel) do { if (gator_annotate) { \
	char gator_header[8]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x06; \
	ANNOTATE_MARSHAL_UINT32(gator_header + 2, channel); \
	ANNOTATE_MARSHAL_UINT16(gator_header + 6, 0); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_NAME_CHANNEL(channel, group, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = strlen(gator_str) & 0xffff; \
	char gator_header[12]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x07; \
	ANNOTATE_MARSHAL_UINT32(gator_header + 2, channel); \
	ANNOTATE_MARSHAL_UINT32(gator_header + 6, group); \
	ANNOTATE_MARSHAL_UINT16(gator_header + 10, gator_str_size); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_NAME_GROUP(group, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = strlen(gator_str) & 0xffff; \
	char gator_header[8]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x08; \
	ANNOTATE_MARSHAL_UINT32(gator_header + 2, group); \
	ANNOTATE_MARSHAL_UINT16(gator_header + 6, gator_str_size); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_VISUAL(data, length, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = strlen(gator_str) & 0xffff; \
	const size_t gator_local_length = length; \
	char gator_header[4]; \
	char gator_length[4]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x04; \
	ANNOTATE_MARSHAL_UINT16(gator_header + 2, gator_str_size); \
	ANNOTATE_MARSHAL_UINT32(gator_length, gator_local_length); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size); \
	ANNOTATE_WRITE(gator_length, sizeof(gator_length)); \
	ANNOTATE_WRITE(data, gator_local_length); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_MARKER() do { if (gator_annotate) { \
	char gator_header[4]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x05; \
	ANNOTATE_MARSHAL_UINT16(gator_header + 2, 0); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_MARKER_STR(str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = strlen(gator_str) & 0xffff; \
	char gator_header[4]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x05; \
	ANNOTATE_MARSHAL_UINT16(gator_header + 2, gator_str_size); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_MARKER_COLOR(color) do { if (gator_annotate) { \
	char gator_header[8]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x05; \
	ANNOTATE_MARSHAL_UINT16(gator_header + 2, 4); \
	ANNOTATE_MARSHAL_UINT32(gator_header + 4, color); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

#define ANNOTATE_MARKER_COLOR_STR(color, str) do { if (gator_annotate) { \
	const char *const gator_str = str; \
	const uint16_t gator_str_size = (strlen(gator_str) + 4) & 0xffff; \
	char gator_header[8]; \
	gator_header[0] = 0x1c; \
	gator_header[1] = 0x05; \
	ANNOTATE_MARSHAL_UINT16(gator_header + 2, gator_str_size); \
	ANNOTATE_MARSHAL_UINT32(gator_header + 4, color); \
	ANNOTATE_LOCK; \
	ANNOTATE_WRITE(gator_header, sizeof(gator_header)); \
	ANNOTATE_WRITE(gator_str, gator_str_size - 4); \
	fflush(gator_annotate); ANNOTATE_UNLOCK; }} while(0)

/* Not to be called by the user */
#define ANNOTATE_MARSHAL_UINT16(buf, val) { \
	char *const gator_marshal_buf = buf; \
	const uint16_t gator_marshal_val = val; \
	gator_marshal_buf[0] = gator_marshal_val & 0xff; \
	gator_marshal_buf[1] = (gator_marshal_val >> 8) & 0xff; \
}

#define ANNOTATE_MARSHAL_UINT32(buf, val) { \
	char *const gator_marshal_buf = buf; \
	const uint32_t gator_marshal_val = val; \
	gator_marshal_buf[0] = gator_marshal_val & 0xff; \
	gator_marshal_buf[1] = (gator_marshal_val >> 8) & 0xff; \
	gator_marshal_buf[2] = (gator_marshal_val >> 16) & 0xff; \
	gator_marshal_buf[3] = (gator_marshal_val >> 24) & 0xff; \
}

#define ANNOTATE_WRITE(data, length) { \
	const char *const gator_data = data; \
	const unsigned int gator_fwrite_length = length; \
	unsigned int gator_pos = 0; \
	while ((gator_pos < gator_fwrite_length) && !feof(gator_annotate) && !ferror(gator_annotate)) { \
		gator_pos += fwrite(&gator_data[gator_pos], 1, gator_fwrite_length - gator_pos, gator_annotate); \
	} \
}

#endif /* _KERNEL_ */
#endif /* STREAMLINE_ANNOTATE_H */
