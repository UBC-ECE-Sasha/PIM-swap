#ifndef __COMMON_H
#define __COMMON_H

#ifndef ON_DPU
#include <linux/types.h>
#endif

/* Everything in this file can be used on the host or the DPU */

#define PAGE_VALID 0x80000000

#define MAX_MRAM_RW 2048

/* To suppress compiler warnings on unused parameters */
#define UNUSED(_x) (_x = _x)

/* To mask out a certain number of bits from a value. For example, to mask the
bottom 12 bits (0xFFF) use: BITMASK(12) */
#define BITMASK(_x) ((1 << _x) - 1)

/* min() and max() functions as macros */
#define MIN(_a, _b) (_a < _b ? _a : _b)
#define MAX(_a, _b) (_a > _b ? _a : _b)

/* Make large numbers easier to read (and accurate) */
#define KILOBYTE(_x) (_x << 10)
#define MEGABYTE(_x) (_x << 20)
#define GIGABYTE(_x) (_x << 30)
#define TERABYTE(_x) (_x << 40)

#define WRAM_SIZE KILOBYTE(64)
#define MRAM_SIZE MEGABYTE(64)

/* If you have a value that needs alignment to the nearest _width. For example,
0xF283 needs aligning to the next largest multiple of 16:
ALIGN(0xF283, 16) will return 0xF290 */
#define DPU_ALIGN(_p, _width) (((unsigned long)_p + (_width - 1)) & (0 - _width))

/* If you need to know an aligned window that contains a given address. For
example, what is the starting address of the 1KB block that contains the address
0xF283?
WINDOW_ALIGN(0xF283, 1024) will return 0xF000 */
#define WINDOW_ALIGN(_p, _width) (((unsigned long)_p) & (0 - _width))

/* Will only print messages (to stdout) when DEBUG is defined */
#ifdef DEBUG
#ifdef ON_DPU
#define dbg_printf(M, ...) printf("%s: " M, __func__, ##__VA_ARGS__)
#else
#define dbg_printf(M, ...) printk("%s: " M, __func__, ##__VA_ARGS__)
#endif
#else
#define dbg_printf(...)
#endif

#endif /* __COMMON_H */
