#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/pgtable.h>

/*
 * colorinfo
 */

struct colorinfo_iter {
	struct colorinfo *ci;
	int nr_entry;
	int cur_entry;
};

static int colorinfo_show(struct seq_file *m, void *v)
{
	int chunk;
	struct colorinfo_iter *iter = (struct colorinfo_iter *) v;
	if (iter == NULL || iter->cur_entry < 0 || iter->cur_entry >= iter->nr_entry) {
		printk("colorinfo_show: invalid iter %lld, iter->cur_entry %d, iter->nr_entry %d\n", (long long int) iter,
		       iter ? iter->cur_entry : 0, iter ? iter->nr_entry : 0);
		return 0;
	}
	for (chunk = 0; chunk < NR_PMEM_CHUNK; ++chunk) {
		seq_printf(m, "numa[%d] - color[%d] (list %d):\tfree %lu, allocated %lu\n",
		           iter->ci[iter->cur_entry].nid, iter->ci[iter->cur_entry].color, chunk,
		           iter->ci[iter->cur_entry].total_free_pages[chunk], iter->ci[iter->cur_entry].total_allocated_pages[chunk]);
	}
	return 0;
}

static void *colorinfo_start(struct seq_file *m, loff_t *pos)
{
	int nr_entry;
	struct colorinfo *ci;
	struct colorinfo_iter *iter;

	m->private = NULL;
	nr_entry = get_colorinfo(&ci);
	if (nr_entry <= 0) {
		printk("colorinfo_start: get_colorinfo error, ret %d\n", nr_entry);
		return NULL;
	}
	if ((*pos) >= nr_entry) {
		kfree(ci);
		return NULL;
	}
	iter = kmalloc(sizeof(struct colorinfo_iter), GFP_KERNEL);
	if (!iter) {
		printk("colorinfo_start: failed to allocate iterator\n");
		kfree(ci);
		return NULL;
	}
	iter->ci = ci;
	iter->nr_entry = nr_entry;
	iter->cur_entry = (int)(*pos);
	m->private = iter;
	return iter;
}

static void *colorinfo_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct colorinfo_iter *iter = (struct colorinfo_iter *) v;
	(*pos)++;
	iter->cur_entry++;
	if (iter->cur_entry >= iter->nr_entry || iter->cur_entry < 0) {
		return NULL;
	}
	return iter;
}

static void colorinfo_stop(struct seq_file *m, void *v)
{
	struct colorinfo_iter *iter = (struct colorinfo_iter *) m->private;
	if (iter) {
		kfree(iter->ci);
		kfree(iter);
	}
}

static struct seq_operations colorinfo_op = {
        .start 	= colorinfo_start,
        .next 	= colorinfo_next,
        .stop 	= colorinfo_stop,
        .show 	= colorinfo_show
};

static int colorinfo_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &colorinfo_op);
}

static const struct proc_ops colorinfo_proc_ops = {
	.proc_open	= colorinfo_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
};

/*
 * pfn_debug
 */

uint64_t pfn_arr[524288];
atomic_t pfn_index;

static int pfn_show(struct seq_file *m, void *v)
{
	uint64_t *pfn_ptr = (uint64_t *) v;
	seq_printf(m, "%lld\n", *pfn_ptr);
	return 0;
}

static void *pfn_start(struct seq_file *m, loff_t *pos)
{
	int local_pfn_index;
	local_pfn_index = atomic_read(&pfn_index);
	if (local_pfn_index < 0) {
		local_pfn_index = 0;
	}
	if (local_pfn_index > 524288) {
		local_pfn_index = 524288;
	}

	if (local_pfn_index == 0) {
		return NULL;
	} else if ((*pos) < 0 || (*pos) >= local_pfn_index) {
		return NULL;
	} else {
		return &pfn_arr[*pos];
	}
}

static void *pfn_next(struct seq_file *m, void *v, loff_t *pos)
{
	uint64_t *pfn_ptr = (uint64_t *) v;
	int local_pfn_index;
	(*pos)++;
	local_pfn_index = atomic_read(&pfn_index);
	if ((*pos) >= local_pfn_index || (*pos) >= 524288 || (*pos) < 0) {
		pfn_ptr = NULL;
	} else {
		pfn_ptr++;
	}
	return pfn_ptr;
}

static void pfn_stop(struct seq_file *m, void *v)
{
	if (!v)
		atomic_set(&pfn_index, 0);
}

static struct seq_operations pfn_op = {
        .start 	= pfn_start,
        .next 	= pfn_next,
        .stop 	= pfn_stop,
        .show 	= pfn_show
};

static int pfn_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pfn_op);
}

static const struct proc_ops pfn_proc_ops = {
	.proc_open	= pfn_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
};

static int __init proc_colorinfo_init(void)
{
	proc_create("colorinfo", 0, NULL, &colorinfo_proc_ops);
	proc_create("pfn_debug", 0, NULL, &pfn_proc_ops);
	return 0;
}
fs_initcall(proc_colorinfo_init);
