#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

static int __init init_pim_swap(void)
{
	printk("%s: hello!", __func__);
	pr_err("init ok\n");

	return 0;
}

static void __exit exit_pim_swap(void)
{
	printk("%s: exit\n", __func__);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
