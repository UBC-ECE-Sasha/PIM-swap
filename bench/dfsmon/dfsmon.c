#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include "dfsmon.h"

int monitor_debugfs(char** paths, char* outputfile, int n_paths, int interval_ms, int n_samples) {
    clock_t start_clk, elapsed_clk;
    int ms_elapsed, ms_sleep;
    uint64_t read_val;

    FILE **fds = malloc(sizeof(FILE*) * n_paths);
    for (int i = 0; i < n_paths; i++) {
        fds[i] = fopen(paths[i], "rb");
        if (fds[i] == NULL) {
            for (int j = 0; j < i; j++) {
                fclose(fds[j]);
            }
            return ENOENT;
        }
    }

    /* Write CSV header */
    FILE *outfile = fopen(outputfile, "w");
    for (int i = 0; i < n_paths; i++) {
        fprintf(outfile, "%s,", paths[i]);
    }
    fprintf(outfile, "\n");

    /* Write rows to CSV */
    for (int i = 0; n_samples && i < n_samples; i++) {
        start_clk = clock();
        for (int j = 0; j < n_paths; j++) {
            read_val = read_uint64(fds[j]);
            fprintf(outfile, "%lu,", read_val);
        }
        fprintf(outfile, "\n");

        elapsed_clk = clock() - start_clk;
        ms_elapsed = (int)(elapsed_clk * 1000 / CLOCKS_PER_SEC);
        ms_sleep = interval_ms - ms_elapsed;
        if (ms_sleep > 0) {
            msleep(ms_sleep);
        }
    }

    for (int i = 0; i < n_paths; i++) {
        fclose(fds[i]);
    }
    return 0;
}

uint64_t read_uint64(FILE* fd) {
    uint64_t val;
    fread(&val, sizeof(uint64_t), 1, fd);
    return val;
}

/* msleep(): Sleep for the requested number of milliseconds. From here https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}