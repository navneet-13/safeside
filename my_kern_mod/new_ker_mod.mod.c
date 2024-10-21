#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x92997ed8, "_printk" },
	{ 0x571f1ef6, "__register_chrdev" },
	{ 0x441d0de9, "__kmalloc_large_noprof" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x37a0cba, "kfree" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0xca83badc, "module_layout" },
};

MODULE_INFO(depends, "");

