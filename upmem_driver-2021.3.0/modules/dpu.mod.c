#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
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

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v00001D0Fd0000F001sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00001D0Fd0000F010sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000FFF0d00001001sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000FFF0d00001000sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "BFF68E2850B2019CB70B821");
