#ifndef __DPU__H
#define _DPU__H

#include <linux/types.h>

#define MB (1024 * 1024)
#define MEM_PER_DPU (64 * MB)
#define DPUS_PER_RANK 64

#define DPU_ALLOCATE_ALL (U32_MAX)

#define DPU_FOREACH(_set, _dpu, _id) \
	for (_id=_set.start; _id < _set.end; _id++) \
		_dpu.rank_id = _set.rank_id; \
		_dpu.start = _id;

/* This definition allows for contiguous subsets of a particular rank, but
	cannot include dpus from different ranks. It's good enough for now, since
	I will always deal with one rank at time. */
struct dpu_set_t {
	uint8_t rank_id;
	uint8_t start;	// if of first dpu from the rank
	uint8_t end;	// id of last dpu in the rank (contiguous)
};

typedef enum dpu_error_t {
    DPU_OK,
    DPU_ERR_INTERNAL,
    DPU_ERR_SYSTEM,
    DPU_ERR_DRIVER,
    DPU_ERR_ALLOCATION,
    DPU_ERR_INVALID_DPU_SET,
    DPU_ERR_INVALID_THREAD_ID,
    DPU_ERR_INVALID_NOTIFY_ID,
    DPU_ERR_INVALID_WRAM_ACCESS,
    DPU_ERR_INVALID_IRAM_ACCESS,
    DPU_ERR_INVALID_MRAM_ACCESS,
    DPU_ERR_INVALID_SYMBOL_ACCESS,
    DPU_ERR_MRAM_BUSY,
    DPU_ERR_TRANSFER_ALREADY_SET,
    DPU_ERR_DIFFERENT_DPU_PROGRAMS,
    DPU_ERR_CORRUPTED_MEMORY,
    DPU_ERR_DPU_DISABLED,
    DPU_ERR_DPU_ALREADY_RUNNING,
    DPU_ERR_INVALID_MEMORY_TRANSFER,
    DPU_ERR_INVALID_LAUNCH_POLICY,
    DPU_ERR_DPU_FAULT,
    DPU_ERR_ELF_INVALID_FILE,
    DPU_ERR_ELF_NO_SUCH_FILE,
    DPU_ERR_ELF_NO_SUCH_SECTION,
    DPU_ERR_TIMEOUT,
    DPU_ERR_INVALID_PROFILE,
    DPU_ERR_UNKNOWN_SYMBOL,
    DPU_ERR_LOG_FORMAT,
    DPU_ERR_LOG_CONTEXT_MISSING,
    DPU_ERR_LOG_BUFFER_TOO_SMALL,
    DPU_ERR_VPD_INVALID_FILE,
} dpu_error_t;

typedef enum _dpu_xfer_t {
    DPU_XFER_TO_DPU,
    DPU_XFER_FROM_DPU,
} dpu_xfer_t;

typedef enum _dpu_xfer_flags_t {
    DPU_XFER_DEFAULT,
    DPU_XFER_NO_RESET,
} dpu_xfer_flags_t;

typedef enum _dpu_launch_policy_t {
    DPU_ASYNCHRONOUS,
    DPU_SYNCHRONOUS,
} dpu_launch_policy_t;


dpu_error_t dpu_alloc(uint32_t nr_dpus, const char *profile, struct dpu_set_t *dpu_set);

dpu_error_t dpu_free(struct dpu_set_t dpu_set);

dpu_error_t dpu_prepare_xfer(struct dpu_set_t dpu_set, void *buffer);

dpu_error_t dpu_push_xfer(struct dpu_set_t dpu_set,
    dpu_xfer_t xfer,
    const char *symbol_name,
    uint32_t symbol_offset,
    size_t length,
    dpu_xfer_flags_t flags);

dpu_error_t dpu_launch(struct dpu_set_t dpu_set, dpu_launch_policy_t policy);

#endif // _DPU__H
