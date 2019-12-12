#ifndef SHMCACHE_SHM_LOCK_H
#define SHMCACHE_SHM_LOCK_H

#include "common_types.h"

class shm_lock {
public:
    static int lock_init(context &context);
    static int read_lock(context &context, const config &config, global_stats &global_stats);
    static int read_unlock(context &context);
    static int write_lock(context &context, const config &config, global_stats &global_stats);
    static int write_unlock(context &context);
    static int file_lock(context &context, const config &config);
    static void file_unlock(context &context);
    static int handle_deadlock(context &context, const config &config, global_stats &global_stats);

private:
    static inline int file_write_lock(int fd);
};

#endif // SHMCACHE_SHM_LOCK_H
