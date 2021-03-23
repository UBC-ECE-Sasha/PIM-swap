#include <linux/slab.h> // kmalloc
#include <linux/mm.h>
#include "dpu.h"
#include "common.h"
#include "snappy_compress.h" // JKN - temporary - this cannot stay here

#define DEBUG_PRINT(...) 

enum {
	STATE_FLAG_RUNNING,
	STATE_FLAG_FAULT,
};

/**** Simulation of DPU state ****/
static int allocated;
static int current_dpu; // currently executing dpu
static struct dpu_state dpus[DPUS_PER_RANK];

/* return current tasklet on current DPU */
int me(void)
{
	return dpus[current_dpu].current_tasklet;
};

int get_current_dpu(void)
{
	return current_dpu;
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
		dpus[i].wram_heap = 0;

		// grab some memory to simulate the DPUs
		dpus[i].mram = vmalloc(MRAM_PER_DPU);
		DEBUG_PRINT("alloc %u MRAM for DPU %i @ %pS\n", MRAM_PER_DPU, i, dpus[i].mram);
		if (!dpus[i].mram) {
			pr_err("%s: can't alloc MRAM for DPU %i\n", __func__, i);

			// undo!
			for (; i >= 0; i--) {
				kfree(dpus[i].mram);
				kfree(dpus[i].wram);
			}

			return DPU_ERR_ALLOCATION;
		}

		// make sure it is zeroed - this is not standard (should not be in the API) but is a shortcut for now
		memset(dpus[i].mram, 0, MRAM_PER_DPU);

		dpus[i].wram = vmalloc(WRAM_SIZE);
		DEBUG_PRINT("alloc %u WRAM for DPU %i @ %pS\n", WRAM_SIZE, i, dpus[i].wram);
		if (!dpus[i].wram) {
			pr_err("%s: can't alloc WRAM for DPU %i\n", __func__, i);

			// undo!
			kfree(dpus[i].mram);
			for (; i >= 0; i--) {
				kfree(dpus[i].mram);
				kfree(dpus[i].wram);
			}

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
	uint32_t nr_dpus;

	//printk("%s\n", __func__);
	nr_dpus = dpu_set.end - dpu_set.start + 1;
	if (nr_dpus != 1)
		return DPU_ERR_INVALID_DPU_SET;

	//printk("%s: Setting xfer buffer %i to %pS\n", __func__, dpu_set.start, buffer);
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

	//printk("%s\n", __func__);
	for (i=dpu_set.start; i <= dpu_set.end; i++) {
		// don't copy empty descriptors
		if (!push_data[i])
			continue;

		// copy the data
		if (xfer == DPU_XFER_TO_DPU) {
			//printk("Transfer to DPU %pS to %pS length %lu\n", push_data[i], MRAM_DPU(i) + symbol_addr + symbol_offset, length);
			memcpy(MRAM_DPU(i) + symbol_addr + symbol_offset, push_data[i], length);
		} else {
			//printk("Transfer to CPU %pS to %pS length %lu\n", MRAM_DPU(i) + symbol_addr + symbol_offset, push_data[i], length);
			memcpy(push_data[i], MRAM_DPU(i) + symbol_addr + symbol_offset, length);
		}

		// reset the buffer
		push_data[i] = 0;
	}

	return DPU_OK;
}

dpu_error_t __dpu_copy_to(struct dpu_set_t dpu_set, uint64_t symbol_addr, uint32_t symbol_offset, const void *src, size_t length)
{
	int i;
	uint32_t nr_dpus = dpu_set.end - dpu_set.start + 1;

	printk("%s\n", __func__);

	for (i=0; i < nr_dpus; i++) {

		printk("DPU %i MRAM at %pS\n", i, dpus[i].mram);
		memcpy(MRAM_DPU(i) + symbol_addr + symbol_offset, src, length);
	}

	return DPU_OK;
}

/** Load a DPU program to a dpu set

	Loading in the simulator is really just setting the main function for each DPU in the set.
	The code actually executes on the host CPU so there is no external binary - just a function.
*/
dpu_error_t dpu_load(struct dpu_set_t dpu_set, dpu_main main, void *prog_dummy)
{
	struct dpu_set_t dpu;
	uint8_t dpu_id;

	//entry = fake_entry;

	DPU_FOREACH(dpu_set, dpu, dpu_id) {
		//printk("Setting DPU %u main=%pS\n", dpu_id, main);
		dpus[dpu_id].main = main;
	}

	return DPU_OK;
}

dpu_error_t dpu_launch(struct dpu_set_t dpu_set, dpu_launch_policy_t policy)
{
	struct dpu_set_t dpu;
	uint8_t dpu_id;

	// ok, I'm running a DPU program now.
	DPU_FOREACH(dpu_set, dpu, dpu_id) {
		current_dpu = dpu_id;
		//printk("Setting current dpu to %u\n", current_dpu);

		for (dpus[dpu_id].current_tasklet=0;
			dpus[dpu_id].current_tasklet < NR_TASKLETS;
			dpus[dpu_id].current_tasklet++) {
			dpus[dpu_id].wram_heap = 0;
			//printk("Launching DPU %u tasklet %u\n", dpu_id, dpus[dpu_id].current_tasklet);
			dpus[dpu_id].main();
		}
	}

	return DPU_OK;
}

/******* WRAM ALLOCATOR *********/
void *mem_alloc_nolock(size_t size)
{
	uint8_t *new_pointer;
	uint32_t pointer = __WRAM_HEAP_POINTER;
	//printk("%s: current pointer 0x%x\n", __func__, pointer);

	if (size) {
		//printk("%s: allocating size 0x%x\n", __func__, size);
		pointer = ((pointer + (8-1)) & (~(8-1)));
		__WRAM_HEAP_POINTER = pointer + size;
		//printk("%s: new pointer 0x%x\n", __func__, __WRAM_HEAP_POINTER);
	}

	//printk("base wram: %pS\n", base);
	new_pointer = get_dpu(get_current_dpu())->wram + pointer;
	//printk("%s: allocated %p\n", __func__, (void*)WRAM_HEAP_VAR(pointer));
	//printk("%s: allocated %pS\n", __func__, new_pointer);
	return new_pointer;
}

void *mem_alloc(size_t size)
{
	// atomic acquire
	void *pointer = mem_alloc_nolock(size);
	// atomic release
	return pointer;
}
