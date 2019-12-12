#ifndef SHMCACHE_COMMON_TYPES_H
#define SHMCACHE_COMMON_TYPES_H

#include "common_define.h"
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/types.h>

#ifdef SHM_AVX_MEMCPY
#include "mem/memcpy_avx.h"
#endif

#ifdef SHM_FOLLY_MEMCPY
extern "C" {
void *memcpy_folly(void *dst, void *src, uint32_t length);
}
#endif

static void memcpy_var(void *dst, void *src, uint32_t length) {
#ifdef SHM_FOLLY_MEMCPY
    memcpy_folly(dst, src, length);
#endif
#ifdef SHM_AVX_MEMCPY
    memcpy_fast(dst, src, length);
#endif
#ifdef SHM_ORIGIN_MEMCPY
    memcpy(dst, src, length);
#endif
}

struct key_info {
    uint32_t length;
    char *data;

    explicit key_info(char *key)
        : length((uint32_t)strlen(key))
        , data(key) {}

    explicit key_info(uint32_t len, char *key)
        : length(len)
        , data(key) {}
};

struct value_info {
    uint32_t length;
    uint32_t options;
    time_t expires;
    char *data;

    explicit value_info(char *val, uint32_t op, uint32_t ttl)
        : length((uint32_t)strlen(val))
        , options(op)
        , expires(ttl != 0 ? 0 : time(nullptr) + ttl)
        , data(val) {}

    explicit value_info(uint32_t len, char *val, uint32_t op, int32_t ttl)
        : length(len)
        , options(op)
        , expires(ttl == 0 ? 0 : (int32_t)time(nullptr) + ttl)
        , data(val) {}
};

struct mem_segment {
    uint32_t id;
    uint32_t size;
    key_t key;
    char *base;

    void reset() {
        id = 0;
        size = 0;
        key = 0;
        base = nullptr;
    }
};

struct ht_segment {
    mem_segment item;
};

struct val_segments {
    uint32_t current;
    mem_segment *items;
};

struct block_addr {
    int32_t index;
    int32_t number;

    block_addr()
        : index(-1)
        , number(-1) {}

    void reset() {
        index = -1;
        number = -1;
    }

    bool valid_addr() { return index != -1 || number != -1; }
};

struct block_entry {
    block_addr next;
    char data[0];

    block_entry()
        : next()
        , data() {}

    void reset() {
        next.index = -1;
        next.number = -1;
    }
};

struct hash_entry {
    uint32_t key_len;
    uint32_t value_len;
    uint32_t options;
    time_t expires;
    int64_t hash_next;
    int64_t lru_prev;
    int64_t lru_next;
    uint32_t popular;
    time_t born;
    uint32_t block_used;
    block_addr first_addr;

    explicit hash_entry(int64_t offset_f2base)
        : key_len(0)
        , value_len(0)
        , options(0)
        , expires(0)
        , hash_next(0)
        , lru_prev(offset_f2base)
        , lru_next(offset_f2base)
        , popular(0)
        , born(0)
        , block_used(0) {}

    void reset(int64_t offset_f2base) {
        key_len = 0;
        value_len = 0;
        options = 0;
        expires = 0;
        hash_next = 0;
        lru_prev = offset_f2base;
        lru_next = offset_f2base;
        popular = 0;
        born = 0;
        block_used = 0;
        first_addr.reset();
    }

    void update(const hash_entry &entry) {
        key_len = entry.key_len;
        value_len = entry.value_len;
        options = entry.options;
        expires = entry.expires;
        hash_next = entry.hash_next;
        lru_prev = entry.lru_prev;
        lru_next = entry.lru_next;
        popular = entry.popular;
        born = entry.born;
        block_used = entry.block_used;
        first_addr = entry.first_addr;
    }

    void write_data(const val_segments &val_segments, const key_info &key_info, const value_info &value_info,
                    uint32_t block_size) {
        key_len = key_info.length;
        value_len = value_info.length;
        options = value_info.options;
        expires = value_info.expires;
        popular = 0;
        born = time(nullptr);

        char *dst;
        block_addr cursor_addr = first_addr;
        auto *cursor_entry =
            (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);
        dst = cursor_entry->data;
        memset(dst, 0, SHM_MEM_ALIGN_BYTE(key_info.length));
        memcpy_var(dst, key_info.data, key_info.length);
        dst += SHM_MEM_ALIGN_BYTE(key_info.length);
        uint32_t rest_of_block = block_size - (uint32_t)sizeof(block_entry) - SHM_MEM_ALIGN_BYTE(key_info.length);
        uint32_t offset = 0;

        while (value_info.length - offset > rest_of_block) {
            memcpy_var(dst, value_info.data + offset, rest_of_block);
            offset += rest_of_block;
            rest_of_block = block_size - (uint32_t)sizeof(block_entry);
            cursor_addr = cursor_entry->next;
            cursor_entry =
                (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);
            dst = cursor_entry->data;
        }
        memcpy_var(dst, value_info.data + offset, value_info.length - offset);
    }

    void read_data(const val_segments &val_segments, value_info &value_info, uint32_t block_size) {
        value_info.length = value_len;
        value_info.options = options;
        value_info.expires = expires;

        char *src;
        block_addr cursor_addr = first_addr;
        auto *cursor_entry =
            (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);
        src = cursor_entry->data + SHM_MEM_ALIGN_BYTE(key_len);

        uint32_t rest_of_block = block_size - (uint32_t)sizeof(block_entry) - SHM_MEM_ALIGN_BYTE(key_len);
        uint32_t offset = 0;

        while (value_len - offset > rest_of_block) {
            memcpy_var(value_info.data + offset, src, rest_of_block);
            offset += rest_of_block;
            rest_of_block = block_size - (uint32_t)sizeof(block_entry);
            cursor_addr = cursor_entry->next;
            cursor_entry =
                (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);
            src = cursor_entry->data;
        }
        memcpy_var(value_info.data + offset, src, value_len - offset);
    }

    int check_entry(const val_segments &val_segments, uint32_t block_size);
};

struct idle_list {
    uint32_t block_size;
    uint32_t block_current;
    int64_t offset_f2base;
    block_entry fake_block;

    explicit idle_list(int64_t offset)
        : block_size(0)
        , block_current(0)
        , offset_f2base(offset) {}

    void reset(const val_segments &val_segments, uint32_t count_of_each) {
        block_current = 0;
        fake_block.reset();
        for (uint32_t index = 0; index < val_segments.current; ++index) {
            add_val_segment(val_segments, index, count_of_each);
        }
    }

    void add_val_segment(const val_segments &val_segments, uint32_t index, uint32_t count) {
        block_addr first_addr = fake_block.next;
        block_entry *prev_entry = &fake_block;
        for (uint32_t number = 0; number < count; ++number) {
            prev_entry->next.index = (int32_t)index;
            prev_entry->next.number = (int32_t)number;
            prev_entry = (block_entry *)(val_segments.items[index].base + number * block_size);
        }
        prev_entry->next = first_addr;
        block_current += count;
    }

    bool alloc_hash_entry_block(const val_segments &val_segments, hash_entry &new_entry, uint32_t block_used) {
        new_entry.block_used = block_used;

        block_addr cursor_addr = fake_block.next;
        if (!cursor_addr.valid_addr()) {
            return false;
        }
        block_entry *cursor_entry = nullptr;

        uint32_t alloc_num = 0;
        do {
            cursor_entry =
                (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);

            cursor_addr = cursor_entry->next;
            ++alloc_num;
        } while (alloc_num < block_used);

        new_entry.first_addr = fake_block.next;

        fake_block.next = cursor_addr;
        cursor_entry->next.reset();

        block_current -= block_used;

        return alloc_num == block_used;
    }

    bool free_hash_entry_block(const val_segments &val_segments, hash_entry &old_entry) {
        block_addr cursor_addr = old_entry.first_addr;
        if (!cursor_addr.valid_addr()) {
            return false;
        }
        block_entry *cursor_entry = nullptr;

        uint32_t free_num = 0;
        do {
            cursor_entry =
                (block_entry *)(val_segments.items[cursor_addr.index].base + (uint32_t)cursor_addr.number * block_size);
            cursor_addr = cursor_entry->next;
            ++free_num;
        } while (cursor_addr.valid_addr());

        cursor_entry->next = fake_block.next;
        fake_block.next = old_entry.first_addr;

        block_current += old_entry.block_used;

        return old_entry.block_used == free_num;
    }

    int check_list(const val_segments &val_segments);
};

struct busy_list {
    uint32_t entry_size;
    uint32_t entry_current;
    int64_t offset_f2base;
    hash_entry fake_entry;

    explicit busy_list(int64_t offset)
        : entry_size(0)
        , entry_current(0)
        , offset_f2base(offset)
        , fake_entry(offset) {}

    void reset() {
        entry_current = 0;
        fake_entry.reset(offset_f2base);
    }

    int check_list(const ht_segment &ht_segment);
};

struct entry_queue {
    uint32_t head;
    uint32_t tail;
    uint32_t capacity;
    int64_t offset_2base;

    explicit entry_queue(int64_t offset)
        : head(0)
        , tail(0)
        , capacity(0)
        , offset_2base(offset) {}

    void reset() { head = tail = 0; }

    void tail_forward() { tail = (tail + 1u + capacity) % capacity; }

    void head_forward() { head = (head + 1u + capacity) % capacity; }
};

struct hashtable {
    uint32_t capacity;
    uint32_t inserted;
    int64_t bucket[0];

    hashtable()
        : capacity(0)
        , inserted(0)
        , bucket() {}

    void reset() {
        inserted = 0;
        memset(&bucket, 0, sizeof(int64_t) * capacity);
    }
};

struct basic_unit {
    struct {
        uint32_t size;
        uint32_t current;
        uint32_t max;
    } segment;
    struct {
        uint32_t size;
        uint32_t max_of_each;
    } block;
};

struct ratio_counter {
    volatile uint32_t total;
    volatile uint32_t success;

    void reset() { total = success = 0; }
};

struct global_stats {
    ratio_counter set;
    ratio_counter get;
    ratio_counter del;
    volatile uint32_t r_lock_total;
    volatile uint32_t r_lock_retry;
    volatile uint32_t w_lock_total;
    volatile uint32_t w_lock_retry;
    volatile uint32_t detect_deadlock;
    volatile uint32_t unlock_deadlock;
    volatile uint32_t survive_duration;
    volatile uint32_t eliminate_count;
    volatile uint32_t get_bytes;
    volatile uint32_t lru_count;
    struct {
        ratio_counter get;
        uint32_t survive_duration;
        uint32_t eliminate_count;
        uint32_t get_bytes;
        uint32_t lru_times;
        time_t time;
    } last;
    time_t last_clear_time;

    void reset() {
        set.reset();
        get.reset();
        del.reset();
        r_lock_total = 0;
        r_lock_retry = 0;
        w_lock_total = 0;
        w_lock_retry = 0;
        detect_deadlock = 0;
        unlock_deadlock = 0;
        survive_duration = 0;
        eliminate_count = 0;
        get_bytes = 0;
        lru_count = 0;
        last.get.reset();
        last.survive_duration = 0;
        last.eliminate_count = 0;
        last.get_bytes = 0;
        last.lru_times = 0;
        last.time = time(nullptr);
        last_clear_time = time(nullptr);
    }
};

struct out_call_counter {
    uint32_t call_count;
    uint32_t call_ok_count;
    uint32_t all_cost;
    uint32_t all_ok_cost;
    uint32_t max_ok_cost;

    void reset() { call_count = call_ok_count = all_cost = all_ok_cost = max_ok_cost = 0; }
};

struct in_call_counter {
    uint32_t call_count;
    uint32_t all_cost;
    uint32_t max_cost;

    void reset() { call_count = all_cost = max_cost = 0; }
};

struct local_stats {
    struct out_call_counter set;
    struct out_call_counter get;
    struct out_call_counter del;
    struct in_call_counter r_data;
    struct in_call_counter w_data;
    struct in_call_counter r_lock;
    struct in_call_counter w_lock;
    uint32_t start_cycle;
    uint32_t end_cycle;
    uint32_t get_bytes;
    double cpu_freq;

    void start() {
        start_cycle = get_cpu_cycle();
        init_cpu_info();
    }

    void stop() { end_cycle = get_cpu_cycle(); }

    void reset() {
        set.reset();
        get.reset();
        del.reset();
        w_data.reset();
        r_data.reset();
        w_lock.reset();
        r_lock.reset();
        start_cycle = end_cycle = 0;
        get_bytes = 0;
        cpu_freq = 0.0;
    }

    void show();

    std::string serialize(bool write, const char *dir);

    void init_cpu_info();

    static uint32_t get_cpu_cycle();
};

struct memory_lock {
    pid_t owner;
    pthread_mutex_t mutex;

    memory_lock()
        : owner(-1)
        , mutex() {}
};

struct memory_info {
    time_t init_time;
    uint32_t size;
    uint32_t max_key_count;
    uint32_t status;

    struct global_stats global_stats;
    struct memory_lock global_lock;
    struct basic_unit basic_unit;
    struct idle_list idle_list;
    struct busy_list busy_list;
    struct entry_queue entry_queue;
    struct hashtable hashtable;
};

struct config {
    uint32_t max_mem_mb;
    uint32_t min_mem_mb;
    uint32_t max_key_count;
    uint32_t segment_size;
    uint32_t block_size;
    uint32_t max_key_size;
    uint32_t max_value_size;
    char file[SHM_MAX_PATH_SIZE];
    char dir[SHM_MAX_PATH_SIZE];
    uint32_t memory_type;
    bool recycle_valid;
    uint32_t try_r_lk_interval;
    uint32_t try_w_lk_interval;
    uint32_t detect_r_dl_ticks;
    uint32_t detect_w_dl_ticks;

    void reset() {
        max_mem_mb = SHM_MAX_MEM_MB;
        min_mem_mb = SHM_MIN_MEM_MB;
        max_key_count = SHM_MAX_KEY_NUM;
        segment_size = SHM_SEGMENT_SIZE;
        block_size = SHM_BLOCK_SIZE;
        max_key_size = SHM_MAX_KEY_SIZE;
        max_value_size = SHM_MAX_VAL_SIZE;
        memset(file, 0, SHM_MAX_PATH_SIZE);
        memset(dir, 0, SHM_MAX_PATH_SIZE);
        memory_type = SHM_MEM_TYPE_MMAP;
        recycle_valid = true;
        try_r_lk_interval = SHM_TRYLOCK_INTERVAL;
        try_w_lk_interval = SHM_TRYLOCK_INTERVAL;
        detect_r_dl_ticks = SHM_TRYLOCK_TICKS;
        detect_w_dl_ticks = SHM_TRYLOCK_TICKS;
    }
};

struct context {
    int lock_fd;
    bool enable_create;
    bool enable_stats;
    struct memory_info *memory;
    struct local_stats local_stats;
    struct ht_segment ht_segment;
    struct val_segments val_segments;

    void reset() {
        lock_fd = -1;
        enable_create = true;
        enable_stats = true;
        memory = nullptr;
        local_stats.reset();
        ht_segment.item.reset();
        val_segments.current = 0;
        val_segments.items = nullptr;
    }
};

struct stats_output {
    struct global_stats global_stats;
    struct {
        time_t seconds;
        double get_ratio;
        double get_qps;
        double survive;
        uint32_t save_bytes;
        uint32_t lru_times;
    } performance;

    void show();

    std::string serialize();
};

#endif // SHMCACHE_COMMON_TYPES_H
