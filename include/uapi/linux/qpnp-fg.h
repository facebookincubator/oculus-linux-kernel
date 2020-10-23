#ifndef _UAPI_QPNP_FG_H_
#define _UAPI_QPNP_FG_H_

#include <linux/types.h>

#define QPNP_FG_BUCKET_COUNT 8

#define QPNP_FG_CYCLE_COUNTS_MAGIC 0xBA77EF1E

struct fg_cycle_bucket {
	uint16_t id;
	uint16_t count;
} __attribute__((__packed__));

struct fg_cycle_counts {
	uint32_t magic;
	uint64_t timestamp;
	uint8_t bucket_count;
	struct fg_cycle_bucket buckets[QPNP_FG_BUCKET_COUNT];
	uint32_t crc32;
} __attribute__((__packed__));
#endif
