/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM damon

#if !defined(_TRACE_DAMON_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DAMON_H

#include <linux/damon.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(damon_aggregated,

	TP_PROTO(struct damon_target *t, unsigned int target_id,
		struct damon_region *r, unsigned int nr_regions,
		unsigned long region_id, long last_aggregation),

	TP_ARGS(t, target_id, r, nr_regions, region_id, last_aggregation),

	TP_STRUCT__entry(
		__field(unsigned long, target_id)
		__field(unsigned int, nr_regions)
		__field(unsigned long, start)
		__field(unsigned long, end)
		__field(unsigned int, nr_accesses)
		__field(unsigned int, age)
		__field(unsigned long, sampling_addr)
		__field(unsigned long, region_id)
		__field(long, last_aggregation)
	),

	TP_fast_assign(
		__entry->target_id = target_id;
		__entry->nr_regions = nr_regions;
		__entry->start = r->ar.start;
		__entry->end = r->ar.end;
		__entry->nr_accesses = r->nr_accesses;
		__entry->age = r->age;
		__entry->sampling_addr = r->sampling_addr;
		__entry->region_id = region_id;
		__entry->last_aggregation = last_aggregation;
	),

	TP_printk("target_id=%lu nr_regions=%u %lu-%lu: %u %u %lu %lu %ld",
			__entry->target_id, __entry->nr_regions,
			__entry->start, __entry->end,
			__entry->nr_accesses, __entry->age,
			__entry->sampling_addr, __entry->region_id,
			__entry->last_aggregation)
);

#endif /* _TRACE_DAMON_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
