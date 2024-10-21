#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "my_device"
#define IOCTL_GET_ADDRESS _IOR('a', 1, unsigned long *)

static char *kernel_buffer = NULL;
static int major;

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_GET_ADDRESS:
            if (copy_to_user((unsigned long *)arg, &kernel_buffer, sizeof(kernel_buffer))) {
                return -EFAULT;
            }
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device opened\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device closed\n");
    return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
    .open = device_open,
    .release = device_release,
};

static int __init my_module_init(void) {
    // Register character device
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        return major;
    }
    printk(KERN_INFO "Device registered with major number %d\n", major);

    // Allocate kernel buffer
    kernel_buffer = (char *)kmalloc(1024 * 1024, GFP_KERNEL);  // 1 MB array
    if (!kernel_buffer) {
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ALERT "Failed to allocate memory\n");
        return -ENOMEM;
    }
    strcpy(kernel_buffer, "Hello from the kernel!");

    return 0;
}

static void __exit my_module_exit(void) {
    if (kernel_buffer) {
        kfree(kernel_buffer);
    }
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Device unregistered\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Navneet");
MODULE_DESCRIPTION("Return address of kernel array to user space.");
