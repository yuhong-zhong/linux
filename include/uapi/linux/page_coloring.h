#ifndef _LINUX_PAGE_COLORING_H
#define _LINUX_PAGE_COLORING_H

#include <linux/types.h>


/* FIXME: update this info whenever try on new machine */
#define DRAM_SIZE_PER_NODE (96ul << 30)  // 96 GB DRAM per NUMA node
#define NR_COLORS 768u  // 768 colors; each color represents 96 GB / 768 = 128 MB DRAM
#define NR_PMEM_CHUNK 8  // Optane to DRAM ratio is 1:8; each DRAM line corresponds to 8 Optane lines


struct ppool_fill_req {
	__u64 target_num_pages;
	int nid;
	unsigned long __user *user_mask_ptr;
};

#define PPOOL_IOC_FILL		_IOW('^', 0, struct ppool_fill_req *)
#define PPOOL_IOC_ENABLE	_IOW('^', 1, int *)
#define PPOOL_IOC_DISABLE	_IOW('^', 2, int *)


struct color_remap_req {
	int pid;
	int nid;
	int num_pages;
	void __user * __user *page_arr;
	unsigned long __user *user_mask_ptr;
	int use_ppool;
};

struct color_swap_req {
	int pid;
	int num_pages;
	void __user * __user *page_arr_1;
	void __user * __user *page_arr_2;
};

#define COLOR_IOC_REMAP		_IOW('?', 0, struct color_remap_req *)
#define COLOR_IOC_SWAP		_IOW('?', 1, struct color_swap_req *)

#endif /* _LINUX_PAGE_COLORING_H */
