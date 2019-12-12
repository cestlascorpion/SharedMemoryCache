#include "shm_hashtable.h"
#include "shm_allocator.h"
#include <algorithm>
#include <cerrno>
#include <unistd.h>
#include <vector>

const std::vector<uint32_t> shm_hashtable::prime_array = {
    1,          /* 0 */
    3,          /* 1 */
    17,         /* 2 */
    37,         /* 3 */
    79,         /* 4 */
    163,        /* 5 */
    331,        /* 6 */
    673,        /* 7 */
    1361,       /* 8 */
    2729,       /* 9 */
    5471,       /* 10 */
    10949,      /* 11 */
    21911,      /* 12 */
    43853,      /* 13 */
    87719,      /* 14 */
    175447,     /* 15 */
    350899,     /* 16 */
    701819,     /* 17 */
    1403641,    /* 18 */
    2807303,    /* 19 */
    5614657,    /* 20 */
    11229331,   /* 21 */
    22458671,   /* 22 */
    44917381,   /* 23 */
    89834777,   /* 24 */
    179669557,  /* 25 */
    359339171,  /* 26 */
    718678369,  /* 27 */
    1437356741, /* 28 */
    2147483647  /* 29 (largest signed int prime) */
};

int shm_hashtable::ht_set(context &context, const config &config, const key_info &key_info,
                          const value_info &value_info) {
    if (context.memory->hashtable.inserted >= config.max_key_count) {
        uint32_t total = SHM_MEM_ALIGN_BYTE(key_info.length) + SHM_MEM_ALIGN_BYTE(value_info.length);
        uint32_t rest_of_block = context.memory->basic_unit.block.size - (uint32_t)sizeof(block_entry);
        uint32_t block_used = (total + rest_of_block - 1) / rest_of_block;
        int res = ht_recycle(context, config, block_used, true);
        if (res != 0) {
            printf("%s %s: pid: %d reach max key count but ht_recycle(force) failed.\n", __FILE__, __func__, getpid());
            return res;
        }
    }
    hash_entry *new_entry = shm_allocator::alloc_hash_entry(context, config, key_info, value_info);
    if (new_entry == nullptr) {
        printf("%s %s: pid: %d alloc_hash_entry() failed.\n", __FILE__, __func__, getpid());
        return -1;
    }
    uint32_t ht_index = bucket_index(context, key_info);
    int64_t old_offset = context.memory->hashtable.bucket[ht_index];
    bool found = false;
    hash_entry *prev_entry = nullptr;
    hash_entry *old_entry = nullptr;
    while (old_offset > 0) {
        old_entry = (hash_entry *)(context.ht_segment.item.base + old_offset);
        if (same_key(context, old_entry, key_info)) {
            found = true;
            break;
        }
        old_offset = old_entry->hash_next;
        prev_entry = old_entry;
    }
    if (found) {
        new_entry->hash_next = old_entry->hash_next;
        int64_t prev_lru_offset = old_entry->lru_prev;
        auto *prev_lru_entry = (hash_entry *)(context.ht_segment.item.base + prev_lru_offset);
        int64_t next_lru_offset = old_entry->lru_next;
        auto *next_lru_entry = (hash_entry *)(context.ht_segment.item.base + next_lru_offset);
        prev_lru_entry->lru_next = next_lru_offset;
        next_lru_entry->lru_prev = prev_lru_offset;
    } else {
        new_entry->hash_next = 0;
    }
    auto new_offset = (char *)new_entry - context.ht_segment.item.base;
    if (prev_entry != nullptr) {
        prev_entry->hash_next = new_offset;
    } else {
        context.memory->hashtable.bucket[ht_index] = new_offset;
    }
    auto *fake_entry = &context.memory->busy_list.fake_entry;
    int64_t last_lru_offset = context.memory->busy_list.fake_entry.lru_prev;
    auto *last_lru_entry = (hash_entry *)(context.ht_segment.item.base + last_lru_offset);
    last_lru_entry->lru_next = new_offset;
    fake_entry->lru_prev = new_offset;
    new_entry->lru_prev = last_lru_offset;
    new_entry->lru_next = context.memory->busy_list.offset_f2base;
    ++context.memory->hashtable.inserted;
    ++context.memory->busy_list.entry_current;
    if (found) {
        if (shm_allocator::free_hash_entry(context, old_offset) != 0) {
            shm_hashtable::ht_clear(context, context.memory->global_stats);
            return -1;
        }
    }
    return 0;
}

int shm_hashtable::ht_set_expires(context &context, const key_info &key_info, uint32_t expires) {
    int res = ENOENT;
    uint32_t ht_index = shm_hashtable::bucket_index(context, key_info);
    int64_t entry_offset = context.memory->hashtable.bucket[ht_index];
    hash_entry *current_entry = nullptr;
    while (entry_offset > 0) {
        current_entry = (hash_entry *)(context.ht_segment.item.base + entry_offset);
        if (shm_hashtable::same_key(context, current_entry, key_info)) {
            if (shm_hashtable::valid_key(current_entry)) {
                current_entry->expires = expires;
                res = 0;
            } else {
                res = ETIMEDOUT;
            }
            break;
        }
        entry_offset = current_entry->hash_next;
    }
    return res;
}

int shm_hashtable::ht_get(context &context, const key_info &key_info, value_info &value_info, uint32_t lru) {
    int res = ENOENT;
    uint32_t ht_index = shm_hashtable::bucket_index(context, key_info);
    int64_t entry_offset = context.memory->hashtable.bucket[ht_index];
    hash_entry *current_entry = nullptr;
    while (entry_offset > 0) {
        current_entry = (hash_entry *)(context.ht_segment.item.base + entry_offset);
        if (!shm_hashtable::same_key(context, current_entry, key_info)) {
            entry_offset = current_entry->hash_next;
        } else {
            if (!shm_hashtable::valid_key(current_entry)) {
                res = ETIMEDOUT;
                break;
            }
            uint32_t read_start{}, read_end{};
            if (context.enable_stats) {
                read_start = local_stats::get_cpu_cycle();
            }
            current_entry->read_data(context.val_segments, value_info, context.memory->basic_unit.block.size);
            if (context.enable_stats) {
                read_end = local_stats::get_cpu_cycle();
                context.local_stats.r_data.all_cost += read_end - read_start;
                ++context.local_stats.r_data.call_count;
                context.local_stats.r_data.max_cost =
                    std::max(read_end - read_start, context.local_stats.r_data.max_cost);
            }
            res = 0;

            if (++current_entry->popular < lru)
                break;
            auto *fake_entry = (hash_entry *)(context.ht_segment.item.base + context.memory->busy_list.offset_f2base);
            int64_t last_offset = fake_entry->lru_prev;
            if (last_offset != entry_offset) {
                auto *last_entry = (hash_entry *)(context.ht_segment.item.base + last_offset);
                last_entry->lru_next = entry_offset;
                fake_entry->lru_prev = entry_offset;
                int64_t prev_lru_offset = current_entry->lru_prev;
                auto *prev_lru_entry = (hash_entry *)(context.ht_segment.item.base + prev_lru_offset);
                int64_t next_lru_offset = current_entry->lru_next;
                auto *next_lru_entry = (hash_entry *)(context.ht_segment.item.base + next_lru_offset);
                prev_lru_entry->lru_next = next_lru_offset;
                next_lru_entry->lru_prev = prev_lru_offset;
                current_entry->lru_next = context.memory->busy_list.offset_f2base;
                current_entry->lru_prev = last_offset;
                ++context.memory->global_stats.lru_count;
            }
            break;
        }
    }
    return res;
}

int shm_hashtable::ht_del(context &context, const key_info &key_info, bool by_recycle) {
    uint32_t ht_index = bucket_index(context, key_info);
    int64_t removed_offset = context.memory->hashtable.bucket[ht_index];
    bool found = false;
    hash_entry *prev_entry = nullptr;
    hash_entry *removed_entry = nullptr;
    while (removed_offset > 0) {
        removed_entry = (hash_entry *)(context.ht_segment.item.base + removed_offset);
        if (same_key(context, removed_entry, key_info)) {
            found = true;
            break;
        }
        removed_offset = removed_entry->hash_next;
        prev_entry = removed_entry;
    }
    if (!found) {
        if (by_recycle) {
            printf("%s %s: pid: %d ht_recycle() call ht_del() failed, clear hashtable...\n", __FILE__, __func__,
                   getpid());
            ht_clear(context, context.memory->global_stats);
        }
        return -1;
    }
    if (prev_entry != nullptr) {
        prev_entry->hash_next = removed_entry->hash_next;
    } else {
        context.memory->hashtable.bucket[ht_index] = removed_entry->hash_next;
    }
    int64_t prev_lru_offset = removed_entry->lru_prev;
    auto *prev_lru_entry = (hash_entry *)(context.ht_segment.item.base + prev_lru_offset);
    int64_t next_lru_offset = removed_entry->lru_next;
    auto *next_lru_entry = (hash_entry *)(context.ht_segment.item.base + next_lru_offset);
    prev_lru_entry->lru_next = removed_entry->lru_next;
    next_lru_entry->lru_prev = removed_entry->lru_prev;
    if (shm_allocator::free_hash_entry(context, removed_offset) != 0) {
        shm_hashtable::ht_clear(context, context.memory->global_stats);
        return -1;
    }
    return 0;
}

int shm_hashtable::ht_recycle(context &context, const config &config, uint32_t block_used, bool force) {
    int64_t current_offset = context.memory->busy_list.fake_entry.lru_next;
    while (current_offset != context.memory->busy_list.offset_f2base) {
        auto *current_entry = (hash_entry *)(context.ht_segment.item.base + current_offset);
        int64_t current_next = current_entry->lru_next;
        if (config.recycle_valid || force || !valid_key(current_entry)) {
            context.memory->global_stats.survive_duration += (uint32_t)(time(nullptr) - current_entry->born);
            ++context.memory->global_stats.eliminate_count;
            char *key_data = context.val_segments.items[current_entry->first_addr.index].base +
                             (uint32_t)current_entry->first_addr.number * context.memory->basic_unit.block.size +
                             sizeof(block_entry);
            key_info temp_key_info(current_entry->key_len, key_data);
            if (ht_del(context, temp_key_info, true) != 0) {
                printf("%s %s: pid: %d ht_del() failed, force = %d.\n", __FILE__, __func__, getpid(), force);
            }
        }
        current_offset = current_next;
        if (context.memory->idle_list.block_current >= block_used) {
            break;
        }
    }
    if (context.memory->idle_list.block_current < block_used) {
        printf("%s %s: pid: %d fail to recycle enough block.\n", __FILE__, __func__, getpid());
        return -1;
    }
    return 0;
}

int shm_hashtable::ht_clear(context &context, global_stats &global_stats) {
    global_stats.last_clear_time = time(nullptr);
    auto cleared_hash_entry = (int)context.memory->busy_list.entry_current;
    context.memory->hashtable.reset();
    context.memory->entry_queue.reset();
    context.memory->idle_list.reset(context.val_segments, context.memory->basic_unit.block.max_of_each);
    context.memory->busy_list.reset();
    return cleared_hash_entry;
}

uint32_t shm_hashtable::get_capacity(uint32_t max_key_count) {
    auto iter = std::upper_bound(prime_array.begin(), prime_array.end(), max_key_count);
    if (iter == prime_array.end()) {
        return prime_array.back();
    }
    return *iter;
}

uint32_t shm_hashtable::simple_hash(const char *key, uint32_t len) {
    uint32_t hash = 0;
    const char *pEnd = key + len;
    for (auto *p = key; p != pEnd; ++p) {
        hash = 31 * hash + (uint32_t)*p;
    }
    return hash;
}

uint32_t shm_hashtable::bucket_index(context &context, const key_info &key_info) {
    uint32_t hash_code = simple_hash(key_info.data, key_info.length);
    return (hash_code % context.memory->hashtable.capacity);
}

bool shm_hashtable::same_key(context &context, hash_entry *old_entry, const key_info &key_info) {
    if (old_entry->key_len != key_info.length) {
        return false;
    }
    const char *key_data = context.val_segments.items[old_entry->first_addr.index].base +
                           (uint32_t)old_entry->first_addr.number * context.memory->basic_unit.block.size +
                           sizeof(block_entry);
    return memcmp(key_data, key_info.data, (uint32_t)key_info.length) == 0;
}

bool shm_hashtable::valid_key(hash_entry *old_entry) {
    return (old_entry->expires == 0 || old_entry->expires > time(nullptr));
}