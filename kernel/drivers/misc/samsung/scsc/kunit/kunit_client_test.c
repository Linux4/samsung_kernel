#define copy_from_user(args...) (0)
#define alloc_chrdev_region(args...) (0)

static int auto_start;
void set_auto_start(int autostart)
{
	auto_start = autostart;
}

static int service_id_2;
void set_service_id_2(int id_2)
{
	service_id_2 = id_2;
}

static u8 test_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err);
u8 (*fp_test_failure_notification)(struct scsc_service_client *client, struct mx_syserr_decode *err) = &test_failure_notification;

static bool test_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err);
bool (*fp_test_stop_on_failure)(struct scsc_service_client *client, struct mx_syserr_decode *err) = &test_stop_on_failure;

static void test_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code);
void (*fp_test_failure_reset)(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code) = &test_failure_reset;

static int client_test_dev_open(struct inode *inode, struct file *file);
int (*fp_client_test_dev_open)(struct inode *inode, struct file *file) = &client_test_dev_open;

static ssize_t client_test_dev_write(struct file *file, const char *data, size_t len, loff_t *offset);
ssize_t (*fp_client_test_dev_write)(struct file *file, const char *data, size_t len, loff_t *offset) = &client_test_dev_write;

static ssize_t client_test_dev_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
ssize_t (*fp_client_test_dev_read)(struct file *filp, char *buffer, size_t length, loff_t *offset) = &client_test_dev_read;

static int client_test_dev_release(struct inode *inode, struct file *file);
int (*fp_client_test_dev_release)(struct inode *inode, struct file *file) = &client_test_dev_release;

static int __init scsc_client_test_module_init(void);
int (*fp_scsc_client_test_module_init)(void) = &scsc_client_test_module_init;

static void __exit scsc_client_test_module_exit(void);
void (*fp_scsc_client_test_module_exit)(void) = &scsc_client_test_module_exit;

static void delay_start_func(struct work_struct *work);
void (*fp_delay_start_func)(struct work_struct *work) = &delay_start_func;