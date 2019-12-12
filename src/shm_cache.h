#ifndef SHMCACHE_SHM_CACHE_H
#define SHMCACHE_SHM_CACHE_H

#include "common_types.h"

class shm_cache {
public:
    shm_cache();
    ~shm_cache();

public:
    int init(const char *file, bool create, bool check);
    int set_ex(char *key, char *value, int ttl, int op);
    int set_ttl_ex(char *key, int ttl);
    int set_expires_ex(char *key, int expires);
    int get_ex(char *key, char *value, int lru);
    int del_ex(char *key);

    int set(const key_info &key_info, const value_info &value_info);
    int set_ttl(const key_info &key_info, uint32_t ttl);
    int set_expires(const key_info &key_info, uint32_t expires);
    int get(const key_info &key_info, value_info &value_info, uint32_t lru);
    int del(const key_info &key_info);

    int destroy();
    int remove();

public:
    int clear_hashtable();
    time_t get_last_ht_clear_time() const;
    stats_output get_global_stats();
    int clear_global_stats();
    void start_local_observe();
    std::string stop_local_observe(bool write2file, bool clear);
    void clear_local_stats();

private:
    void reset();
    int load_config(const char *file);
    int do_init(bool create, bool check);
    int do_lock_init(const basic_unit &basic_unit, const hashtable &hashtable, uint32_t &offset_2base);

private:
    inline int check_ht_segment(const basic_unit &basic_unit, uint32_t &offset_2base) const;
    inline void get_unit_and_ht(basic_unit &basic_unit, hashtable &hashtable, uint32_t &total_size,
                                uint32_t &offset_2base);
    inline void calc_basic_uint(basic_unit &basic_uint, uint64_t max_memory);
    inline int check_consistence();

private:
    config m_config;
    context m_context;
};

#endif // SHMCACHE_SHM_CACHE_H