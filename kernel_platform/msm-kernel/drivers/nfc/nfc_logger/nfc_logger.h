#ifndef _NFC_LOGGER_H_
#define _NFC_LOGGER_H_

#ifdef CONFIG_SEC_NFC_LOGGER

#define NFC_LOG_ERR(fmt, ...) \
	do { \
		pr_err("sec_nfc: "fmt, ##__VA_ARGS__); \
		nfc_logger_print(fmt, ##__VA_ARGS__); \
	} while (0)
#define NFC_LOG_INFO(fmt, ...) \
	do { \
		pr_info("sec_nfc: "fmt, ##__VA_ARGS__); \
		nfc_logger_print(fmt, ##__VA_ARGS__); \
	} while (0)
#define NFC_LOG_DBG(fmt, ...) \
	do { \
		pr_debug("sec_nfc: "fmt, ##__VA_ARGS__); \
		nfc_logger_print(fmt, ##__VA_ARGS__); \
	} while (0)
#define NFC_LOG_REC(fmt, ...)		nfc_logger_print(fmt, ##__VA_ARGS__)

void nfc_logger_set_max_count(int count);
void nfc_logger_print(const char *fmt, ...);
void nfc_print_hex_dump(void *buf, void *pref, size_t len);
int nfc_logger_init(void);
void nfc_logger_deinit(void);
void nfc_logger_register_nfc_stauts_func(void (*nfc_status_func)(void));
#endif

#ifdef CONFIG_SEC_NFC_LOGGER_ADD_ACPM_LOG
u32 nfc_logger_acpm_get_rtc_time(void);
void nfc_logger_acpm_log_print(void);
void nfc_logger_acpm_log_init(u32 rtc_addr);
#endif

#endif
