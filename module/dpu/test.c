
#include <mram.h>
#include <stdio.h>

#include "../common.h"

#define XFER_SIZE_BYTES 4096

uint8_t __mram_noinit buf[XFER_SIZE_BYTES];

int main(void) {
	printf("Ran");
}
