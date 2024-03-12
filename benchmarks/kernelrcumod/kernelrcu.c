#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* for sprintf() */
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/version.h>

#include <asm/errno.h>

int rcu_data[2] = {8, 0};
int* gp = &rcu_data[0];
volatile int ongoing = 0;
atomic_long_t read_cnt = ATOMIC_LONG_INIT(0);
atomic_long_t write_cnt = ATOMIC_LONG_INIT(0);

/*  Prototypes - this would normally go in a .h file */
static int device_open(struct inode*, struct file*);
static int device_release(struct inode*, struct file*);
static ssize_t device_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t device_write(struct file*, const char __user*, size_t, loff_t*);

#define SUCCESS 0
#define DEVICE_NAME "kernelrcu" /* Dev name as it appears in /proc/devices */
#define BUF_LEN 80              /* Max length of the message from the device */

/* Global variables are declared as static, so are global within the file. */

static int major; /* major number assigned to our device driver */

enum {
  CDEV_NOT_USED = 0,
  CDEV_EXCLUSIVE_OPEN = 1,
};

static struct class* cls;

static struct file_operations chardev_fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

static int __init chardev_init(void) {
  major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

  if (major < 0) {
    pr_alert("Registering char device failed with %d\n", major);
    return major;
  }

  pr_info("I was assigned major number %d.\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
  cls = class_create(DEVICE_NAME);
#else
  cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
  device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

  pr_info("Device created on /dev/%s\n", DEVICE_NAME);

  return SUCCESS;
}

static void __exit chardev_exit(void) {
  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);

  /* Unregister the device */
  unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/chardev"
 */
static int device_open(struct inode* inode, struct file* file) {
  try_module_get(THIS_MODULE);
  return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode* inode, struct file* file) {
  /* Decrement the usage count, or else once you opened the file, you will
   * never get rid of the module.
   */
  ongoing = 0;
  module_put(THIS_MODULE);
  return SUCCESS;
}

/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file* filp,   /* see include/linux/fs.h   */
                           char __user* buffer, /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t* offset) {
  int res = 0;
  if (length == 0) {
    ongoing = 2;
    return 0;
  }

  if (length == 3) {
    pr_info("printing result\n");
    pr_info("%ld, %ld\n", atomic_long_read(&read_cnt),
            atomic_long_read(&write_cnt));

    unsigned long rread_cnt = atomic_long_read(&read_cnt);
    unsigned long rwrite_cnt = atomic_long_read(&write_cnt);
    copy_to_user(buffer, &rread_cnt, sizeof(unsigned long));
    copy_to_user(buffer + sizeof(unsigned long), &rwrite_cnt,
                 sizeof(unsigned long));
    return 0;
  }

  while (ongoing == 0)
    ;
  pr_info("read test start\n");
  /* Number of bytes actually written to the buffer */
  for (;;) {
    int* p;
    rcu_read_lock();
    p = rcu_dereference(gp);
    BUG_ON(*p != 8);
    rcu_read_unlock();
    res += 1;
    if (ongoing != 1)
      break;
  }

  atomic_long_add(res, &read_cnt);
  return res;
}

static DEFINE_MUTEX(rcu_mutex);
int cur = 0;
/* Called when a process writes to dev file: echo "hi" > /dev/hello */
static ssize_t device_write(struct file* filp, const char __user* buff,
                            size_t len, loff_t* off) {
  int res = 0;

  if (len == 0) {  // begin test
    ongoing = 1;
    return 0;
  }

  while (ongoing == 0)
    ;

  pr_info("write test start\n");
  for (;;) {
    int *newp, *oldp;
    mutex_lock(&rcu_mutex);
    cur = !cur;
    newp = &rcu_data[cur];
    *newp = 8;
    oldp = rcu_dereference(gp);
    rcu_assign_pointer(gp, newp);
    synchronize_rcu();
    *oldp = 0;
    mutex_unlock(&rcu_mutex);
    res += 1;
    if (ongoing != 1)
      break;
  }

  atomic_long_add(res, &write_cnt);
  return len;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
