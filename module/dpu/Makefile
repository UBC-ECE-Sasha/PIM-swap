CC           = dpu-upmem-dpurte-clang
CFLAGS       = --std=c11 -O2 -flto -g -Wall

# define DEBUG in the source if we are debugging
ifeq ($(DEBUG), 1)
	CFLAGS+=-DDEBUG
endif

CFLAGS += -DON_DPU

INVALIDATE_SOURCES = dpu_invalidate.c alloc_static.c
INVALIDATE_DPU = invalidate.dpu

DECOMPRESS_SOURCES = lzo_decompress.c alloc_static.c
DECOMPRESS_DPU = decompress.dpu

COMPRESS_SOURCES = lzo_compress.c alloc_static.c
COMPRESS_DPU = compress.dpu

TEST_SOURCES = test.c
TEST_DPU = test.dpu

.PHONY: default all clean

default: all

all: $(INVALIDATE_DPU) $(DECOMPRESS_DPU) $(COMPRESS_DPU) $(TEST_DPU)

clean:
	$(RM) *.dpu

$(INVALIDATE_DPU): $(INVALIDATE_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

$(DECOMPRESS_DPU): $(DECOMPRESS_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

$(COMPRESS_DPU): $(COMPRESS_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_DPU): $(TEST_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@
