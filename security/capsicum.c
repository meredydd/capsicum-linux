#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>


static int capsicum_init(void)
{
	printk("capsicum_init()");
	return 0;
}
module_init(capsicum_init);

