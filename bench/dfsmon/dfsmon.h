#include <stdio.h>
#include <stdint.h>

int monitor_debugfs(char** paths, char* outputfile, int n_paths, int interval_ms, int n_samples);
int read_write_str(FILE* input_fd, FILE* csv_fd);
int msleep(long msec);
