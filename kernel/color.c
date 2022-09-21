/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 Jeongseob Ahn
 * Copyright (c) 2022 Yuhong Zhong
 */

#include <linux/syscalls.h>
#include <linux/colormask.h>
#include <linux/kernel.h>
#include <linux/threads.h>

const DECLARE_BITMAP(color_all_bits, NR_COLORS) = COLOR_BITS_ALL;
EXPORT_SYMBOL(color_all_bits);

static struct task_struct *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_vpid(pid) : current;
}

void do_set_colors_allowed_ptr(struct task_struct *p, const struct colormask *new_mask)
{
	colormask_copy(&p->colors_allowed, new_mask);
}

int set_colors_allowed_ptr(struct task_struct *p, const struct colormask *new_mask)
{
	int ret = 0;

	do_set_colors_allowed_ptr(p, new_mask);
	p->preferred_color = 0;

	return ret;
}

long set_color(pid_t pid, const struct colormask *in_mask)
{
	struct task_struct *p;
	int retval;

	rcu_read_lock();
	p = find_process_by_pid(pid);
	if (!p) {
		printk("color: unable to find the process\n");
		retval = -ESRCH;
		goto unlock;
	}

	retval = set_colors_allowed_ptr(p, in_mask);
unlock:
	rcu_read_unlock();
	return retval;
}

long get_color(pid_t pid, struct colormask *mask)
{
	struct task_struct *p;
	int retval = 0;

	rcu_read_lock();
	p = find_process_by_pid(pid);
	if (!p) {
		printk("color: unable to find the process\n");
		retval = -ESRCH;
		goto unlock;
	}

	colormask_copy(mask, &p->colors_allowed);
unlock:
	rcu_read_unlock();
	return retval;
}

int get_user_color_mask(unsigned long __user *user_mask_ptr, unsigned len, struct colormask *new_mask)
{
	if (len < colormask_size())
		colormask_clear(new_mask);
	else if (len > colormask_size())
		len = colormask_size();

	return copy_from_user(new_mask, user_mask_ptr, len) ? -EFAULT : 0;
}

/**
 * sys_set_color - set the page color of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to the new color mask
 *
 * Return: 0 on success. An error code otherwise.
 */
SYSCALL_DEFINE3(set_color, pid_t, pid, unsigned int, len,
		unsigned long __user *, user_mask_ptr)
{
	colormask_var_t new_mask;
	int ret;

	ret = get_user_color_mask(user_mask_ptr, len, new_mask);
	if (ret == 0)
		ret = set_color(pid, new_mask);

	return 0;
}


/**
 * sys_get_color - get the page color of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to hold the current color mask
 *
 * Return: 0 on success. An error code otherwise.
 */
SYSCALL_DEFINE3(get_color, pid_t, pid, unsigned int, len,
		unsigned long __user *, user_mask_ptr)
{
	int ret;
	colormask_var_t mask;

	if ((len * BITS_PER_BYTE) < NR_COLORS)
		return -EINVAL;
	if (len & (sizeof(unsigned long)-1))
		return -EINVAL;

	ret = get_color(pid, mask);

	if (ret == 0) {
		size_t retlen = min_t(size_t, len, colormask_size());
		if (copy_to_user(user_mask_ptr, mask, retlen)) {
			ret = -EFAULT;
		}
		else {
			ret = retlen;
		}
	}

	return ret;
}

SYSCALL_DEFINE1(reserve_color, long, nr_page)
{
	int nid;
	for_each_online_node(nid)
		rebalance_colormem(nid, nr_page);
	return 0;
}
