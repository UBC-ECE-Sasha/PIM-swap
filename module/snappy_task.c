//JKN #include <mram.h>
//JKN #include <defs.h>
//JKN #include <perfcounter.h>
//JKN #include <stdio.h>
//JKN #include "alloc.h"
#include <linux/printk.h>
#include "snappy_compress.h"
#include "dpu.h"

#define printf printk

// WRAM variables
__host uint32_t block_size;
__host uint32_t input_length;
__host uint32_t output_length[NR_TASKLETS];
__host uint32_t input_block_offset[NR_TASKLETS];
__host uint32_t output_offset[NR_TASKLETS];

int snappy_main(void)
{
	struct in_buffer_context input;
	struct out_buffer_context output;
	uint32_t input_start;
	uint32_t output_start;
	uint8_t idx = me();

	printf("DPU starting, tasklet %d\n", idx);
	
	// Check that this tasklet has work to run
	if ((idx != 0) && (input_block_offset[idx] == 0)) {
		//printf("Tasklet %d has nothing to run\n", idx);
		output_length[idx] = 0;
		return 0;
	}

	// Prepare the input and output descriptors
	input_start = (input_block_offset[idx] - input_block_offset[0]) * block_size;
	output_start = output_offset[idx] - output_offset[0];

	input.buffer = MRAM_VAR(input_buffer) + input_start;
	input.cache = seqread_alloc();
	input.ptr = seqread_init(input.cache, MRAM_VAR(input_buffer) + input_start, &input.sr);
	input.curr = 0;
	input.length = 0;

	output.buffer = MRAM_VAR(output_buffer) + output_start;
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

	if (input.length != 0) {
		// Do the uncompress
		if (dpu_compress(&input, &output, block_size))
		{
			return -1;
		}
		output_length[idx] = output.length;
	}

	return 0;
}

