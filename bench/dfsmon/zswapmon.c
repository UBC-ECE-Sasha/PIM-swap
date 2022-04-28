#include <stdint.h>
#include <stdio.h>
#include "dfsmon.h"

int main(int argc, char *argv[])) {
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
        "/sys/kernel/debug/zswap/pool_limit_hit"
    };
    monitor_debugfs(paths, "zswap_stats.csv", 10, 1000, 0);

    return 0;
}