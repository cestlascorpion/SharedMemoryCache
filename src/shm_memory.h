#ifndef SHMCACHE_SHM_MEMORY_H
#define SHMCACHE_SHM_MEMORY_H

#include "common_types.h"

class shm_memory {
public:
    static void *map(uint32_t type, const char *file, uint32_t id, uint32_t size, key_t &key, bool create, int &error);
    static int unmap(uint32_t type, void *addr, uint32_t size);
    static int remove(uint32_t type, const char *file, uint32_t id, key_t key);
    static bool exists(uint32_t type, const char *file, uint32_t id);

private:
    static inline void *do_mmap(const char *file, uint32_t id, uint32_t size, bool create, int &error);
    static inline void *do_shmmap(key_t &key, uint32_t size, bool create, int &error);
    static inline void get_true_file(char *true_file, const char *file, uint32_t id);
    static inline int get_key(const char *file, uint32_t id, key_t &key);
    static inline int write_file(const char *file, const char *buff, uint32_t size);
};

#endif // SHMCACHE_SHM_MEMORY_H
