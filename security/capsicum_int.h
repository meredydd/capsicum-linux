#ifndef __CAPSICUM_INT_H__
#define __CAPSICUM_INT_H__

#include <linux/file.h>

int capsicum_is_cap(const struct file * file);

int capsicum_wrap_new(struct file * orig, u64 rights);

struct file * capsicum_unwrap(const struct file * capability, u64 * rights);

#endif //__CAPSICUM_INT_H__

