#include <tee_client_api.h>

#define TIMA_UTIL_SRV_UUID  \
     { \
         0xfcfcfcfc, 0x0000, 0x0000, \
         {  \
             0x00, 0x00, 0x00, 0x00, \
             0x00, 0x00, 0x00, 0x15, \
         }, \
     }

#define	SECURE_LOG_SIZE	(1<<16)
#define	DEBUG_LOG_SIZE	(1<<17)
#define	DEBUG_LOG_MAGIC	(0xaabbccdd)
#define	DEBUG_LOG_ENTRY_SIZE	128
typedef struct debug_log_entry_s
{
    uint32_t	timestamp;          /* timestamp at which log entry was made*/
    uint32_t	logger_id;          /* id is 1 for tima, 2 for lkmauth app  */
#define	DEBUG_LOG_MSG_SIZE	(DEBUG_LOG_ENTRY_SIZE - sizeof(uint32_t) - sizeof(uint32_t))
    char	log_msg[DEBUG_LOG_MSG_SIZE];      /* buffer for the entry                 */
} __attribute__ ((packed)) debug_log_entry_t;

typedef struct debug_log_header_s
{
    uint32_t	magic;              /* magic number                         */
    uint32_t	log_start_addr;     /* address at which log starts          */
    uint32_t	log_write_addr;     /* address at which next entry is written*/
    uint32_t	num_log_entries;    /* number of log entries                */
    char	padding[DEBUG_LOG_ENTRY_SIZE - 4 * sizeof(uint32_t)];
} __attribute__ ((packed)) debug_log_header_t;

#define SVC_TIMAUTIL_ID                       0x00070000
#define TIMAUTIL_CREATE_CMD(x) (SVC_TIMAUTIL_ID | x)

/**
 * Commands for TIMA UTIL application.
 * */
typedef enum
{
  TIMAUTIL_DEBUG_LOG_WRITE        = TIMAUTIL_CREATE_CMD(0x00000000),
  TIMAUTIL_DEBUG_LOG_READ     = TIMAUTIL_CREATE_CMD(0x00000010),
  TIMAUTIL_SECURE_LOG_WRITE        = TIMAUTIL_CREATE_CMD(0x00000020),
  TIMAUTIL_SECURE_LOG_READ        = TIMAUTIL_CREATE_CMD(0x00000030),
} timautil_cmd_type;

#define DRIVER_DESC   "A kernel module to read tima debug log"
