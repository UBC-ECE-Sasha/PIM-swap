#include <linux/slab.h> // kmalloc
#include <linux/mm.h>
#include "dpu.h"
#include "common.h"

enum {
	STATE_FLAG_RUNNING,
	STATE_FLAG_FAULT,
};

/**** Simulation of DPU state ****/
static int allocated;
static int current_dpu; // currently executing dpu
struct dpu_state dpus[DPUS_PER_RANK];

/* return current tasklet on current DPU */
int me(void)
{
	return dpus[current_dpu].current_tasklet;
};

struct dpu_state *get_current_dpu(void)
{
	return &dpus[current_dpu];
}

struct dpu_state *get_dpu(uint8_t index)
{
	if (index > DPUS_PER_RANK)
		return NULL;

	return &dpus[index];
}

/**** Simulation of DPU API ****/
static void* push_data[DPUS_PER_RANK];

dpu_error_t dpu_alloc(uint32_t nr_dpus, const char *profile, struct dpu_set_t *dpu_set)
{
	int i;
	uint64_t needed;
	struct sysinfo mem_stats;

	// only one allocation is allowed in this simple emulation
	if (allocated)
		return DPU_ERR_INTERNAL;

	allocated = 1;

	if (nr_dpus == DPU_ALLOCATE_ALL)
		nr_dpus = DPUS_PER_RANK;

	dpu_set->rank_id = 0;
	dpu_set->start = 0;
	dpu_set->end = nr_dpus - 1;

	// make sure we have enough memory - not very accurate but better than nothing
	needed = nr_dpus * (MRAM_PER_DPU + WRAM_SIZE);
	si_meminfo(&mem_stats);
	if ((mem_stats.totalram * PAGE_SIZE) < needed) {
		pr_err("Not enough RAM. You need at least %llu to simulate %u dpus (have %lu)\n", needed, nr_dpus, (mem_stats.totalram * PAGE_SIZE));
		return DPU_ERR_ALLOCATION;
	}

	for (i=0; i < nr_dpus; i++) {
		dpus[i].flags = 0;

		// grab some memory to simulate the DPUs
		pr_err("alloc %u MRAM for DPU %i\n", MRAM_PER_DPU, i);
		dpus[i].mram = vmalloc(MRAM_PER_DPU);
		if (!dpus[i].mram) {
			pr_err("%s: can't alloc MRAM for DPU %i\n", __func__, i);

			// undo!
			for (; i >= 0; i--)
				kfree(dpus[i].mram);

			return DPU_ERR_ALLOCATION;
		}
	}

	return DPU_OK;
}

dpu_error_t dpu_free(struct dpu_set_t dpu_set)
{
	int i;
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	for (i=0; i < nr_dpus; i++) {
		kfree(dpus[i].mram);
	}

	allocated = 0;

	return DPU_OK;
}

dpu_error_t dpu_prepare_xfer(struct dpu_set_t dpu_set, void *buffer)
{
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;
	if (nr_dpus != 1)
		return DPU_ERR_INVALID_DPU_SET;

	push_data[dpu_set.start] = buffer;
	return DPU_OK;
}

dpu_error_t
__dpu_push_xfer(struct dpu_set_t dpu_set,
    dpu_xfer_t xfer,
    uint64_t symbol_addr,
    uint32_t symbol_offset,
    size_t length,
    dpu_xfer_flags_t flags)
{
	int i;
	//uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	for (i=dpu_set.start; i < dpu_set.end; i++) {
		// copy the data
		if (xfer == DPU_XFER_TO_DPU)
			memcpy(MRAM_DPU(i) + symbol_addr + symbol_offset, push_data[i], length);
		else
			memcpy(push_data[i], MRAM_DPU(i) + symbol_addr + symbol_offset, length);

		// reset the buffer
		push_data[i] = 0;
	}

	return DPU_OK;
}

dpu_error_t dpu_copy_to(struct dpu_set_t dpu_set, const char *symbol_name, uint32_t symbol_offset, const void *src, size_t length)
{
	int i;
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	for (i=0; i < nr_dpus; i++) {

		printk("DPU %i MRAM at %p\n", i, dpus[i].mram);
	}

	return DPU_OK;
}

dpu_error_t dpu_load(struct dpu_set_t dpu_set, dpu_main main, void *prog_dummy)
{
	int i;
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	for (i=0; i < nr_dpus; i++) {
		dpus[i].main = main;
	}

	return DPU_OK;
}

dpu_error_t dpu_launch(struct dpu_set_t dpu_set, dpu_launch_policy_t policy)
{
	int i;
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	// ok, I'm running a DPU program now.
	for (i=0; i < nr_dpus; i++) {
		current_dpu = i;
		printk("Launching DPU %i\n", i);
		barrier();
		dpus[i].main();
	}

	// finished!

	return DPU_OK;
}

static unsigned long _WRAM_HEAP_POINTER;
/******* WRAM ALLOCATOR *********/
void *mem_alloc_nolock(size_t size)
{
	unsigned long pointer = _WRAM_HEAP_POINTER;

	if (size) {
		pointer = ((pointer + (8-1)) & (~(8-1)));
		_WRAM_HEAP_POINTER = pointer + size;
	}

	return (void*)pointer;
}

void *mem_alloc(size_t size)
{
	// atomic acquire
	void *pointer = mem_alloc_nolock(size);
	// atomic release
	return pointer;
}
