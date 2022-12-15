/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 Jeongseob Ahn
 * Copyright (c) 2022 Yuhong Zhong
 */
#ifndef __LINUX_COLORMASK_H
#define __LINUX_COLORMASK_H

#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/bitmap.h>
#include <uapi/linux/page_coloring.h>

// #define COLOR_THP  // Enable coloring for Transparent Huge Page

#ifdef COLOR_THP
#define COLOR_PAGE_SHIFT HPAGE_PMD_SHIFT
#else
#define COLOR_PAGE_SHIFT PAGE_SHIFT
#endif
#define COLOR_PAGE_SIZE (1UL << COLOR_PAGE_SHIFT)
#define COLOR_PAGE_ORDER (COLOR_PAGE_SHIFT - PAGE_SHIFT)

/*
 * Can only put up to 75% = (3 * 2 ^ 28) / ((DRAM_SIZE_PER_NODE / NR_COLORS) * NR_PMEM_CHUNK)
 * of available pages to the coloring pool, otherwise the host will OOM
 */
#define NR_COLOR_PAGE_MAX ((1ul << (28 - COLOR_PAGE_SHIFT)) * 3)
#define COLOR_ALLOC_MAX_ATTEMPT 16384

// Number of PFNs to record for debugging
#define COLOR_NR_PFNS 524288

typedef struct colormask { DECLARE_BITMAP(bits, NR_COLORS); } colormask_t;

#define colormask_bits(maskp) ((maskp)->bits)

#define to_colormask(bitmap)							\
	((struct colormask *)(1 ? (bitmap)					\
	                      : (void *)sizeof(__check_is_bitmap(bitmap))))

extern const DECLARE_BITMAP(color_all_bits, NR_COLORS);
#define color_all_mask to_colormask(color_all_bits)

#define COLOR_MASK_LAST_WORD	BITMAP_LAST_WORD_MASK(NR_COLORS)

#if NR_COLORS <= BITS_PER_LONG
#define COLOR_BITS_ALL						\
{								\
	[BITS_TO_LONGS(NR_COLORS)-1] = COLOR_MASK_LAST_WORD	\
}
#else
#define COLOR_BITS_ALL						\
{								\
	[0 ... BITS_TO_LONGS(NR_COLORS)-2] = ~0UL,		\
	[BITS_TO_LONGS(NR_COLORS)-1] = COLOR_MASK_LAST_WORD	\
}
#endif

#define for_each_color(color, mask)				\
	for ((color) = -1;					\
	     (color) = colormask_next((color), (mask)),		\
	     (color) < NR_COLORS;)

#define for_each_color_from(color, mask)			\
	for (;							\
	     (color) = colormask_next((color), (mask)),		\
	     (color) < NR_COLORS;)

#define color_isset(color, colormask) test_bit((color), (colormask).bits)

/* use colormask as a stack variable (instead of allocating from heap),
 * treat it like a pointer
 */
typedef struct colormask colormask_var_t[1];

static inline size_t colormask_size(void)
{
	return BITS_TO_LONGS(NR_COLORS) * sizeof(long);
}

static inline void colormask_clear(struct colormask *dstp)
{
	bitmap_zero(colormask_bits(dstp), NR_COLORS);
}

static inline void colormask_copy(struct colormask* dstp,
                                  const struct colormask *srcp)
{
	bitmap_copy(colormask_bits(dstp), colormask_bits(srcp), NR_COLORS);
}

static inline bool colormask_empty(const struct colormask *srcp)
{
	return bitmap_empty(colormask_bits(srcp), NR_COLORS);
}

static inline unsigned int colormask_first(const struct colormask *srcp)
{
	return find_first_bit(colormask_bits(srcp), NR_COLORS);
}

static inline unsigned int colormask_next(int n, const struct colormask *srcp)
{
	return find_next_bit(colormask_bits(srcp), NR_COLORS, n + 1);
}

static inline unsigned int colormask_weight(const struct colormask *srcp)
{
	return bitmap_weight(colormask_bits(srcp), NR_COLORS);
}

int set_colors_allowed_ptr(struct task_struct *p, const struct colormask *new_mask);

int get_user_color_mask(unsigned long __user *user_mask_ptr, unsigned len, struct colormask *new_mask);

int color_remap(struct color_remap_req *req, colormask_t *colormask);

int color_swap(struct color_swap_req *req);

struct colorinfo {
	__u32 nid;
	__u32 color;
	__kernel_ulong_t total_free_pages[NR_PMEM_CHUNK];
	__kernel_ulong_t total_allocated_pages[NR_PMEM_CHUNK];
};

#endif
