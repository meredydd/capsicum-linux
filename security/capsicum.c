#include <linux/anon_inodes.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/security.h>
#include <linux/syscalls.h>

#include "capsicum_int.h"



struct capability {
	u64 rights;
	struct file *underlying;
};

int enabled;


extern const struct file_operations capability_ops;
extern struct security_operations capsicum_security_ops;




static int __init capsicum_init(void)
{
	printk("capsicum_init()\n");
	enabled = security_module_enable(&capsicum_security_ops);
	if(enabled)
		register_security(&capsicum_security_ops);
	else
		printk("Capsicum not enabled\n");

	return 0;
}
__initcall(capsicum_init);


static int sys_cap_new_impl(unsigned int orig_fd, u64 new_rights)
{
	struct file *file;
	struct files_struct *files = current->files;
	int ret = 0;
	u64 existing_rights = (u64)-1;

	rcu_read_lock();
	file = fcheck_files(files, orig_fd);

	if(file && capsicum_is_cap(file))
		file = capsicum_unwrap(file, &existing_rights);

	if(file && !atomic_long_inc_not_zero(&file->f_count))
		file = NULL;

	rcu_read_unlock();

	if(!file)
		ret = -EBADF;
	else
		ret = capsicum_wrap_new(file, new_rights & existing_rights);

	return ret;
}

SYSCALL_DEFINE2(cap_new, unsigned int, orig_fd, u64, new_rights)
{
	return sys_cap_new_impl(orig_fd, new_rights);
}

SYSCALL_DEFINE0(cap_enter)
{
	panic("cap_enter() is not defined!");
}


int capsicum_is_cap(const struct file *file)
{
	return file->f_op == &capability_ops;
}

/*
 * Create a new file representing a capability, to wrap the original file
 * passed in, with the given rights. If orig is already a capability, the
 * new capability will refer to the underlying capability, rather than
 * creating a chain. Returns the fd number.
 */
int capsicum_wrap_new(struct file * orig, u64 rights)
{
	int fd;
	struct capability * cap;

	if(capsicum_is_cap(orig)) {
		orig = capsicum_unwrap(orig, NULL);
	}

	cap = kmalloc(sizeof(*cap), GFP_KERNEL);
	if (cap == NULL)
		return -ENOMEM;

	cap->rights = rights;
	cap->underlying = orig;
	get_file(orig);

	fd = anon_inode_getfd("[capability]", &capability_ops, cap, 0);

	if(fd < 0) {
		kfree(cap);
		fput(orig);
	}

	return fd;
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


static struct file * capsicum_file_lookup(unsigned int fd, struct file *file)
{
	/* TODO unwrapping is currently unconditional. This needs fixing. */
	struct file * unwrapped = capsicum_unwrap(file, NULL);
	if(unwrapped != NULL)
		file = unwrapped;

	return file;
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


struct security_operations capsicum_security_ops = {
		.name = "capsicum",
		.file_lookup = capsicum_file_lookup
};
