#ifndef _LINUX_PAGE_COLORING_H
#define _LINUX_PAGE_COLORING_H

#include <linux/types.h>

#define COLOR_THP  // Enable coloring for Transparent Huge Page

/* FIXME: update this info whenever try on new machine */
#define DRAM_SIZE_PER_NODE ((127ul << 30) + (1ul << 29))  // 127.5 GB local DDR per NUMA node
#define NR_COLORS 1020  // 1020 colors; each color represents 127.5 GB / 1020 = 128 MB local DDR
#define NR_PMEM_CHUNK 2  // Local DDR to phys mem ratio is 1:2; each local DDR line corresponds to 2 phys mem lines
#define NR_PPOOLS 1u


struct ppool_fill_req {
	int pool;
	__u64 target_num_pages;
	int nid;
	unsigned long __user *user_mask_ptr;
};

struct ppool_enable_req {
	int pool;
	int pid;
};

struct ppool_release_req {
	int pool;
	__u64 num_pages;
	__u64 __user *phys_addr_arr;
};

#define PPOOL_IOC_FILL		_IOW('^', 0, struct ppool_fill_req *)
#define PPOOL_IOC_ENABLE	_IOW('^', 1, struct ppool_enable_req *)
#define PPOOL_IOC_DISABLE	_IOW('^', 2, int *)
#define PPOOL_IOC_RELEASE	_IOW('^', 3, struct ppool_release_req *)


struct color_remap_req {
	int pid;
	int nid;
	int num_pages;
	void __user * __user *page_arr;
	unsigned long __user *user_mask_ptr;
	int preferred_color;
	int use_ppool;
	int ppool;

	int num_get_page_err;
	int num_add_page_err;
	int num_migrate_err;
};

struct color_swap_req {
	int pid_1;
	int pid_2;
	int num_pages;
	void __user * __user *page_arr_1;
	void __user * __user *page_arr_2;
	bool swap_ppool_index;

	void __user * __user *skipped_page_arr_1;
	void __user * __user *skipped_page_arr_2;

	int num_get_page_err;
	int num_add_page_err;
	int num_skipped_page;
	int num_malloc_err;
	int num_migrate_err;
	int num_succeeded;
	int num_thp_succeeded;
};

struct color_fake_remap_req {
	int pid;
	int num_pages;
	void __user * __user *page_arr;

	int num_get_page_err;
	int num_add_page_err;
	int num_skipped_page;
	int num_malloc_err;
	int num_migrate_err;
	int num_succeeded;
	int num_thp_succeeded;
};

#define COLOR_IOC_REMAP		_IOWR('?', 0, struct color_remap_req *)
#define COLOR_IOC_SWAP		_IOWR('?', 1, struct color_swap_req *)
#define COLOR_IOC_FAKE_REMAP	_IOWR('?', 2, struct color_fake_remap_req *)
#define COLOR_IOC_PREP		_IO('?', 3)

#endif /* _LINUX_PAGE_COLORING_H */
