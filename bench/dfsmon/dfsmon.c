#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "dfsmon.h"

#define DEBUGFS_PATH "/sys/kernel/debug/"

int monitor_debugfs(char** paths, char* outputfile, int n_paths, int interval_ms, int n_samples) {
    clock_t start_clk, elapsed_clk;
    struct tm *tm;
    char time_str[128];
    time_t t;
    int ms_elapsed, ms_sleep;
    int64_t read_val;

    /* Write CSV header */
    FILE *outfile = fopen(outputfile, "w");
    char *name;
    fprintf(outfile, "%s,", "timestamp");
    for (int i = 0; i < n_paths; i++) {
        name = paths[i];
        if (strncmp(name, DEBUGFS_PATH, strlen(DEBUGFS_PATH)) == 0) {
            name += strlen(DEBUGFS_PATH);
        }
        fprintf(outfile, "%s,", name);
    }
    fprintf(outfile, "\n");

    /* Write rows to CSV */
    for (int i = 0; !n_samples || i < n_samples; i++) {
        start_clk = clock();
        t = time(NULL);
        tm = localtime(&t);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
        fprintf(outfile, "%s,", time_str);
        for (int j = 0; j < n_paths; j++) {
            FILE *fd = fopen(paths[j], "r");
            if (fd == NULL) {
                printf("Bad file %s\n", paths[i]);
                errno = ENOENT;
                return -1;
            }
            int n = read_write_str(fd, outfile);
            fclose(fd);
        }
        fprintf(outfile, "\n");
        fflush(outfile);
        
        elapsed_clk = clock() - start_clk;
        ms_elapsed = (int)(elapsed_clk * 1000.0 / CLOCKS_PER_SEC);
        // printf("%d us elapsed\n", (int) (elapsed_clk * 1000000.0 / CLOCKS_PER_SEC));
        ms_sleep = interval_ms - ms_elapsed;
        if (ms_sleep > 0) {
            msleep(ms_sleep);
        }
    }

    return 0;
}

int read_write_str(FILE* input_fd, FILE* csv_fd) {
    char buf[1024];
    int n_read = fscanf(input_fd, "%s", buf);
    //rewind(input_fd);
    fprintf(csv_fd, "%s,", buf);
    return n_read;
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