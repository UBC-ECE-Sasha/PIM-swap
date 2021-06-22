//JKN #include <mram.h>
//JKN #include <defs.h>
//JKN #include <perfcounter.h>
//JKN #include <stdio.h>
#include "alloc_static.h"
#include "snappy_compress.h"
#include <stdio.h>

// TODO, set from host
__host uint32_t dpu_id;

// WRAM variables
__host uint32_t block_size;
__host uint32_t input_length;
__host uint32_t output_length[NR_TASKLETS];
__host uint32_t input_block_offset[NR_TASKLETS];
__host uint32_t output_offset[NR_TASKLETS];

/** allocate some storage in the MRAM for this compressed data */
static int alloc_storage_dynamic(int size)
{
	return 100;
}

snappy_status kernel_compress(struct in_buffer_context *input, struct out_buffer_context *output, uint32_t block_size)
{
	return SNAPPY_OK;
}

int snappy_compress_main(void)
{
	struct in_buffer_context input;
	struct out_buffer_context output;
	uint32_t input_start;
	uint32_t output_start;
	uint8_t idx;

	idx = me();
	printf("DPU starting compress, tasklet %d\n", idx);

	// Check that this tasklet has work to run
	if ((idx != 0) && (input_block_offset[idx] == 0)) {
		printf("Tasklet %d has nothing to run\n", idx);
		output_length[idx] = 0;
		return 0;
	}

	input_block_offset[0] = 0;
	block_size = 4096; // should be 32K but we don't have that much data!
	input_length = 4096; // one 4K page to be compressed

	// Prepare the input and output descriptors
	input_start = (input_block_offset[idx] - input_block_offset[0]) * block_size;
	printf("input start: %u\n", input_start);
	output_start = alloc_storage_dynamic(4096);
	//output_start = output_offset[idx] - output_offset[0];

	printf("in_page: 0x%llx\n", (uint64_t)trans_page);
	input.buffer = (uint8_t*)(trans_page) + input_start);
	printf("input.buffer: %p\n", input.buffer);
	input.cache = seqread_alloc();
	input.ptr = seqread_init(input.cache, input.buffer, &input.sr);
	input.curr = 0;
	input.length = 0;

		print_hex_dump(KERN_ERR, "in_page: ", DUMP_PREFIX_ADDRESS,
		    16, 1, get_dpu(dpu_id)->mram + (uintptr_t)input.buffer, 32, true);

	output.buffer = (uint8_t*)(MRAM_VAR(storage) + output_start);
	output.append_ptr = (uint8_t*)DPU_ALIGN(mem_alloc(OUT_BUFFER_LENGTH), 8);
	output.append_window = 0;
	output.curr = 0;
	output.length = 0;

	// Calculate the length this tasklet parses
	if (idx < (NR_TASKLETS - 1)) {
		int32_t input_end = (input_block_offset[idx + 1] - input_block_offset[0]) * block_size;

		// If the end position is zero, then the next task has no work
		// to run. Use the remainder of the input length to calculate this
		// task's length.
		if (input_end <= 0) {
			input.length = input_length - input_start;
		}
		else {
			input.length = input_end - input_start;
		}
	}
	else {
		input.length = input_length - input_start;
	}

	printf("Input length: %u\n", input.length);

	if (input.length != 0) {
		//if (dpu_compress(&input, &output, block_size))
		if (kernel_compress(&input, &output, block_size))
		{
			return -1;
		}
		output_length[idx] = output.length;
	}

	return 0;
}
