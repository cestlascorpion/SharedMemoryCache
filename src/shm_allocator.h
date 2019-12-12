#ifndef SHMCACHE_SHM_ALLOCATOR_H
#define SHMCACHE_SHM_ALLOCATOR_H

#include "common_types.h"

class shm_allocator {
public:
    static int init_ht_segment(uint32_t type, const char *file, mem_segment &segment, uint32_t id, uint32_t size,
                               bool create);
    static int init_val_segment(uint32_t type, const char *file, mem_segment &segment, uint32_t index, uint32_t size,
                                bool create);
    static int create_val_segment(context &context, const config &config);
    static int open_val_segment(context &context, const config &config);
    static int remove_all(uint32_t type, const char *file, ht_segment &ht_segment, val_segments &val_segments,
                          bool create);
    static hash_entry *alloc_hash_entry(context &context, const config &config, const key_info &key_info,
                                        const value_info &value_info);
    static int free_hash_entry(context &context, int64_t removed_offset);

private:
    static hash_entry *do_alloc_hash_entry(context &context, uint32_t block_used, const key_info &key_info,
                                           const value_info &value_info);
};

#endif // SHMCACHE_SHM_ALLOCATOR_H
