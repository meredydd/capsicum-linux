#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "capsicum_int.h"

#include "test_harness.h"


FIXTURE(new_cap) {
	struct file *orig;
	int cap;
	struct file *capf;
};

FIXTURE_SETUP(new_cap) {
	self->orig = fget(0);
	ASSERT_NE(self->orig, NULL);
	self->cap = capsicum_wrap_new(self->orig, 0);
	ASSERT_GT(self->cap, 0);
	self->capf = fcheck(self->cap);
	ASSERT_NE(self->capf, NULL);
}

FIXTURE_TEARDOWN(new_cap) {
	fput(self->orig);
	sys_close(self->cap);
}

TEST_F(new_cap, init_ok) {
	u64 rights = -1;
	struct file *f;

	EXPECT_GT(file_count(self->orig), 1);
	EXPECT_EQ(file_count(self->capf), 1);

	f = capsicum_unwrap(self->capf, &rights);
	EXPECT_EQ(rights, 0);
	EXPECT_EQ(f, self->orig);
}

TEST_F(new_cap, rewrap) {
	struct file *f, *uw;
	u64 rights;

	int old_count = file_count(self->orig);

	int fd = capsicum_wrap_new(self->capf, -1);
	ASSERT_GT(fd, 0);
	f = fcheck(fd);

	uw = capsicum_unwrap(f, &rights);
	EXPECT_EQ(rights, -1);
	EXPECT_EQ(uw, self->orig);

	EXPECT_EQ(file_count(self->orig), old_count + 1);

	fput(f);

	EXPECT_EQ(file_count(self->orig), old_count);
}

TEST_F(new_cap, is_cap) {
	EXPECT_TRUE(capsicum_is_cap(self->capf));
	EXPECT_FALSE(capsicum_is_cap(self->orig));
}

FIXTURE(fget) {
	struct file * orig;
	int cap;

	int orig_refs;
};

FIXTURE_SETUP(fget) {
	self->orig = fget(0);
	self->orig_refs = file_count(self->orig);
	self->cap = capsicum_wrap_new(self->orig, 0);
	ASSERT_EQ(file_count(self->orig), self->orig_refs+1);
	ASSERT_EQ(file_count(fcheck(self->cap)), 1);
}

FIXTURE_TEARDOWN(fget) {
	ASSERT_EQ(file_count(self->orig), self->orig_refs+1);
	sys_close(self->cap);
	ASSERT_EQ(file_count(self->orig), self->orig_refs);
	fput(self->orig);
	ASSERT_EQ(file_count(self->orig), self->orig_refs-1);
}

TEST_F(fget, fget) {
	struct file *f = fget(self->cap);

	EXPECT_EQ(f, self->orig);
	EXPECT_EQ(file_count(fcheck(self->cap)), 1);
	EXPECT_EQ(file_count(self->orig), self->orig_refs+2);

	fput(f);
}

TEST_F(fget, fget_light) {
	int fpn, fpn2;
	struct file *f = fget_light(self->cap, &fpn);

	EXPECT_EQ(f, self->orig);
	EXPECT_FALSE(fpn);
	EXPECT_EQ(file_count(self->orig), self->orig_refs+1);

	fput_light(f, fpn);
}

TEST_F(fget, fget_raw) {
	struct file *f = fget_raw(self->cap);

	EXPECT_EQ(f, self->orig);
	EXPECT_EQ(file_count(fcheck(self->cap)), 1);
	EXPECT_EQ(file_count(self->orig), self->orig_refs+2);

	fput(f);
}

TEST_F(fget, fget_raw_light) {
	int fpn;
	struct file *f = fget_raw_light(self->cap, &fpn);


	EXPECT_EQ(f, self->orig);
	EXPECT_EQ(fpn, 0);
	EXPECT_EQ(file_count(self->orig), self->orig_refs+1);

	fput_light(f, fpn);
}




/*
 * Debugfs shims below here to trigger tests by name
 */


static ssize_t run_test_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char test[128];

	size_t s = min(count, (size_t)127);

	copy_from_user(test, ubuf, s);
	test[s] = '\0';
	if(s > 0 && test[s-1] == '\n')
		test[s-1] = '\0';

	printk("Running tests beginning with '%s':\n", test);
	test_harness_run(test);

	return count;
}

static struct file_operations fops;

static int capsicum_test_init(void)
{
	printk("capsicum_test_init()");

	fops = debugfs_file_operations;
	fops.write = run_test_write;

	debugfs_create_file("run_capsicum_tests", 0644, NULL,
		NULL, &fops);

	return 0;
}
__initcall(capsicum_test_init);


