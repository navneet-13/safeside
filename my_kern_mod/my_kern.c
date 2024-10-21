#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>


// char *my_function(int hell){
    
//     return array;
// }

static int __init my_module_init(void) {
    char array[] = "Hello my Friend";
    printk(KERN_INFO "Kernel address: %p\n, %s", array, (char *)0x00000000bf249283);  // Replace with your target function
    return 0;
}

static void __exit my_module_exit(void) {
    printk(KERN_INFO "Module exiting\n");
}

module_init(my_module_init);
module_exit(my_module_exit);
MODULE_LICENSE("GPL");