#include <stdio.h>
#include <stdint.h>

int monitor_debugfs(char** paths, char* outputfile, int n_paths, int interval_ms, int n_samples);
uint64_t read_uint64(FILE* fd);
int msleep(long msec);
