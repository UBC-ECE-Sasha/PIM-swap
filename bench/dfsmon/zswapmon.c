#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dfsmon.h"

int main(int argc, char *argv[]) {
    char* path = "zswap_stats.csv";
    int ms_period = 1000;
    int n_loops = 0;

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [path] [ms_period] [n_loops]\n", argv[0]);
            return 0;
        }
        else {
            path = argv[1];
        }
    }
    if (argc > 2) {
        ms_period = atoi(argv[2]);
    }
    if (argc > 3) {
        n_loops = atoi(argv[3]);
    }

    char* paths[] = {
        "/sys/kernel/debug/zswap/same_filled_pages",
        "/sys/kernel/debug/zswap/stored_pages",
        "/sys/kernel/debug/zswap/pool_total_size",
        "/sys/kernel/debug/zswap/duplicate_entry",
        "/sys/kernel/debug/zswap/written_back_pages",
        "/sys/kernel/debug/zswap/reject_compress_poor",
        "/sys/kernel/debug/zswap/reject_kmemcache_fail",
        "/sys/kernel/debug/zswap/reject_alloc_fail",
        "/sys/kernel/debug/zswap/reject_reclaim_fail",
        "/sys/kernel/debug/zswap/pool_limit_hit",
        "/sys/kernel/debug/frontswap/failed_stores",
        "/sys/kernel/debug/frontswap/invalidates",
        "/sys/kernel/debug/frontswap/loads",
        "/sys/kernel/debug/frontswap/succ_stores"
    };
    monitor_debugfs(paths, path, 14, ms_period, n_loops);
    return 0;
}