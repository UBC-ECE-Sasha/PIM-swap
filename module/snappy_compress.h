/**
 * Compression on DPU.
 */

#ifndef _DPU_COMPRESS_H_
#define _DPU_COMPRESS_H_

//JKN #include "common.h"
#include "dpu.h"
#include "seqread.h"

#define NR_TASKLETS 12
#define STACK_SIZE_DEFAULT 256

// Length of the "append window" in out_buffer_context
#define OUT_BUFFER_LENGTH 256

// Sequential reader cache size must be the same as the
// append window size, since we memcpy from one to the other
#undef SEQREAD_CACHE_SIZE
#define SEQREAD_CACHE_SIZE OUT_BUFFER_LENGTH

// Return values
typedef enum {
    SNAPPY_OK = 0,              // Success code
    SNAPPY_INVALID_INPUT,       // Input file has an invalid format
    SNAPPY_BUFFER_TOO_SMALL     // Input or output file size is too large
} snappy_status;

// Snappy tag types
enum element_type
{
    EL_TYPE_LITERAL,
    EL_TYPE_COPY_1,
    EL_TYPE_COPY_2,
    EL_TYPE_COPY_4
};

typedef struct in_buffer_context
{
	__mram_ptr uint8_t *buffer;
	uint8_t *ptr;
	seqreader_buffer_t cache;
	seqreader_t sr;
	uint32_t curr;
	uint32_t length;
} in_buffer_context;

typedef struct out_buffer_context
{
	__mram_ptr uint8_t *buffer; // Entire buffer in MRAM
	uint8_t *append_ptr; 		// Append window in MRAM
	uint32_t append_window;		// Offset of output buffer mapped by append window
	uint32_t curr;				// Current offset in output buffer in MRAM
	uint32_t length;			// Total size of output buffer in bytes
} out_buffer_context;

// MRAM buffers
MRAM_VARS_START
uint8_t __mram_noinit input_buffer[MEGABYTE(30)];
uint8_t __mram_noinit in_page[KILOBYTE(4)];
uint8_t __mram_noinit output_buffer[MEGABYTE(30)];
MRAM_VARS_END

/**
 * Perform the Snappy compression on the DPU.
 *
 * @param input: holds input buffer information
 * @param output: holds output buffer information
 * @param block_size: size to compress at a time
 * @return SNAPPY_OK if successful, error code otherwise
 */
snappy_status dpu_compress(struct in_buffer_context *input, struct out_buffer_context *output, uint32_t block_size);

#endif

