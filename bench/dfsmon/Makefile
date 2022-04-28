CC=gcc
CFLAGS=-I.
DEPS = zswapmon.h dfsmon.h
OBJ = dfsmon.o zswapmon.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

zswapmon: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)