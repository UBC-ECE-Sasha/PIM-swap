#include <linux/slab.h> // kmalloc
#include "dpu.h"

/**** Simulation of DPU API ****/

static char *dpu_mem=0;

dpu_error_t dpu_alloc(uint32_t nr_dpus, const char *profile, struct dpu_set_t *dpu_set)
{
	// only one allocation is allowed in this simple emulation
	if (dpu_mem)
		return DPU_ERR_INTERNAL;

	if (nr_dpus == DPU_ALLOCATE_ALL)
		nr_dpus = DPUS_PER_RANK;

	dpu_set->rank_id = 0;
	dpu_set->start = 0;
	dpu_set->end = nr_dpus - 1;

	// grab some memory to simulate the DPUs
	dpu_mem = kmalloc(nr_dpus * MEM_PER_DPU, GFP_KERNEL);
	if (!dpu_mem) {
		pr_err("%s: can't alloc DPU memory\n", __func__);
		return DPU_ERR_ALLOCATION;
	}

	return DPU_OK;
}

dpu_error_t dpu_free(struct dpu_set_t dpu_set)
{
	kfree(dpu_mem);
	dpu_mem = 0;
	return DPU_OK;
}

dpu_error_t dpu_prepare_xfer(struct dpu_set_t dpu_set, void *buffer)
{
	//dpu_set.buffer = buffer;
	return DPU_OK;
}

dpu_error_t
dpu_push_xfer(struct dpu_set_t dpu_set,
    dpu_xfer_t xfer,
    const char *symbol_name,
    uint32_t symbol_offset,
    size_t length,
    dpu_xfer_flags_t flags)
{
	// memcpy something
	return DPU_OK;
}
