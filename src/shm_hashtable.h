#ifndef SHMCACHE_SHM_HASHTABLE_H
#define SHMCACHE_SHM_HASHTABLE_H

#include "common_types.h"
#include <vector>

class shm_hashtable {
public:
    static int ht_set(context &context, const config &config, const key_info &key_info, const value_info &value_info);
    static int ht_set_expires(context &context, const key_info &key_info, uint32_t expires);
    static int ht_get(context &context, const key_info &key_info, value_info &value_info, uint32_t lru);
    static int ht_del(context &context, const key_info &key_info, bool by_recycle);
    static int ht_recycle(context &context, const config &config, uint32_t block_used, bool force);
    static int ht_clear(context &context, global_stats &global_stats);
    static uint32_t get_capacity(uint32_t max_key_count);
    static uint32_t simple_hash(const char *key, uint32_t len);
    static uint32_t bucket_index(context &context, const key_info &key_info);
    static bool same_key(context &context, hash_entry *old_entry, const key_info &key_info);
    static bool valid_key(hash_entry *old_entry);

private:
    static const std::vector<uint32_t> prime_array;
};

#endif // SHMCACHE_SHM_HASHTABLE_H
