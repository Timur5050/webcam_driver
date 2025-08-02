#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x1c\x00\x00\x00\xba\xd8\x4e\x3d"
	"usb_register_driver\0"
	"\x18\x00\x00\x00\x25\x87\x4f\x90"
	"usb_deregister\0\0"
	"\x14\x00\x00\x00\x45\x3a\x23\xeb"
	"__kmalloc\0\0\0"
	"\x18\x00\x00\x00\x0e\x95\xb5\x86"
	"usb_alloc_urb\0\0\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x18\x00\x00\x00\x86\x4f\x51\xbd"
	"usb_kill_urb\0\0\0\0"
	"\x18\x00\x00\x00\xc7\xf3\xd1\x9a"
	"usb_free_urb\0\0\0\0"
	"\x28\x00\x00\x00\xb3\x1c\xa2\x87"
	"__ubsan_handle_out_of_bounds\0\0\0\0"
	"\x1c\x00\x00\x00\x63\xa5\x03\x4c"
	"random_kmalloc_seed\0"
	"\x18\x00\x00\x00\x57\x21\x74\xcb"
	"kmalloc_caches\0\0"
	"\x18\x00\x00\x00\x1a\x3f\x1d\xfe"
	"kmalloc_trace\0\0\0"
	"\x14\x00\x00\x00\xfa\x77\xdf\x84"
	"usb_get_dev\0"
	"\x1c\x00\x00\x00\xfc\xe9\x9a\x17"
	"usb_set_interface\0\0\0"
	"\x14\x00\x00\x00\xe2\x7c\x60\x14"
	"usb_put_dev\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x18\x00\x00\x00\xb0\x70\x10\xbc"
	"usb_submit_urb\0\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x18\x00\x00\x00\x41\x40\xb4\x0e"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v1B3Fp2247d*dc*dsc*dp*ic0Eisc02ip00in*");

MODULE_INFO(srcversion, "BB9F71E770CDD3BDF23631D");
