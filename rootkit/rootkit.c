#include <linux/module.h>
#include <linux/printk.h>

static int rootkit_init(void)
{
	pr_err("rootkit successfully loaded\n");
	return 0;
}

static void rootkit_exit(void)
{
}

module_init(rootkit_init);
module_exit(rootkit_exit);
