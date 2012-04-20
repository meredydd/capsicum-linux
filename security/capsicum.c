#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/slab.h>

#include "capsicum_int.h"



struct capability {
	u64 rights;
	struct file *underlying;
};

extern const struct file_operations capability_ops;




static int capsicum_init(void)
{
	printk("capsicum_init()");
	return 0;
}
__initcall(capsicum_init);


int capsicum_is_cap(const struct file *file)
{
	return file->f_op == &capability_ops;
}

/*
 * Create a new file representing a capability, to wrap the original file
 * passed in, with the given rights. If orig is already a capability, the
 * new capability will refer to the underlying capability, rather than
 * creating a chain.
 */
struct file * capsicum_wrap_new(struct file * orig, u64 rights)
{
	struct file * f = NULL;
	struct capability * cap;

	if(capsicum_is_cap(orig)) {
		orig = capsicum_unwrap(orig, NULL);
	}

	cap = kmalloc(sizeof(*cap), GFP_KERNEL);
	if (cap == NULL)
		return NULL;

	cap->rights = rights;
	cap->underlying = orig;
	get_file(orig);

	f = anon_inode_getfile("[capability]", &capability_ops, cap, 0);

	if(IS_ERR(f))
		goto err_out;

	return f;

err_out:
	kfree(cap);
	return f;
}

/*
 * Given a capability, return the underlying file. If rights is non-NULL,
 * the capability's rights will be stored there too. If cap is not a
 * capability, returns NULL.
 */
struct file * capsicum_unwrap(const struct file * cap, u64 * rights)
{
	struct capability * c;

	if (!capsicum_is_cap(cap))
		return NULL;

	c = cap->private_data;

	if (rights != NULL)
		*rights = c->rights;

	return c->underlying;

	return NULL;
}



static int capsicum_release(struct inode *i, struct file *fp)
{
	struct capability * c;

	if(!capsicum_is_cap(fp)) {
		return EINVAL;
	}

	c = fp->private_data;
	fput(c->underlying);
	
	kfree(c);

	return 0;
}





static void do_panic(void) {
	panic("Called a file_operations member on a Capsicum wrapper");
}

#define panic_ptr (void *)&do_panic

const struct file_operations capability_ops = {
	.owner = NULL,
	.llseek = panic_ptr,
	.read = panic_ptr,
	.write = panic_ptr,
	.aio_read = panic_ptr,
	.aio_write = panic_ptr,
	.readdir = panic_ptr,
	.poll = panic_ptr,
	.unlocked_ioctl = panic_ptr,
	.compat_ioctl = panic_ptr,
	.mmap = panic_ptr,
	.open = panic_ptr,
	.flush = NULL,  /* This one is called on close if implemented. */
	.release = capsicum_release, /* This is the only one we want. */
	.fsync = panic_ptr,
	.aio_fsync = panic_ptr,
	.fasync = panic_ptr,
	.lock = panic_ptr,
	.sendpage = panic_ptr,
	.get_unmapped_area = panic_ptr,
	.check_flags = panic_ptr,
	.flock = panic_ptr,
	.splice_write = panic_ptr,
	.splice_read = panic_ptr,
	.setlease = panic_ptr
};



