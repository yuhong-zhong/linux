#ifndef _LINUX_PAGE_COLORING_H
#define _LINUX_PAGE_COLORING_H

#include <linux/types.h>

struct ppool_fill_req {
	__u64 target_num_pages;
	int nid;
	unsigned long __user *user_mask_ptr;
};

#define PPOOL_IOC_FILL		_IOW('^', 0, struct ppool_fill_req *)
#define PPOOL_IOC_ENABLE	_IOW('^', 1, int *)
#define PPOOL_IOC_DISABLE	_IOW('^', 2, int *)

#endif /* _LINUX_PAGE_COLORING_H */
