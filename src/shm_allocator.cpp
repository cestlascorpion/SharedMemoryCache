#include "shm_allocator.h"
#include "shm_hashtable.h"
#include "shm_memory.h"
#include <cerrno>
#include <unistd.h>

int shm_allocator::init_ht_segment(uint32_t type, const char *file, mem_segment &segment, uint32_t id, uint32_t size,
                                   bool create) {
    int res;
    segment.base = (char *)shm_memory::map(type, file, id, size, segment.key, create, res);
    if (segment.base == nullptr) {
        printf("%s %s: pid: %d map() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    segment.id = id;
    segment.size = size;
    return res;
}

int shm_allocator::init_val_segment(uint32_t type, const char *file, mem_segment &segment, uint32_t index,
                                    uint32_t size, bool create) {
    int res;
    segment.base = (char *)shm_memory::map(type, file, index + 2, size, segment.key, create, res);
    if (segment.base == nullptr) {
        printf("%s %s: pid: %d map() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    segment.id = index + 2;
    segment.size = size;
    return res;
}

int shm_allocator::create_val_segment(context &context, const config &config) {
    int res;
    uint32_t index = context.val_segments.current;
    if (index >= context.memory->basic_unit.segment.max) {
        printf("%s %s: pid: %d map() failed.\n", __FILE__, __func__, getpid());
        return ENOSPC;
    }
    if (index != context.memory->basic_unit.segment.current) {
        printf("%s %s: pid: %d map() failed.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    res = shm_allocator::init_val_segment(config.memory_type, config.file, context.val_segments.items[index], index,
                                          context.memory->basic_unit.segment.size, context.enable_create);
    if (res != 0) {
        printf("%s %s: pid: %d init_val_segment() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    context.memory->idle_list.add_val_segment(context.val_segments, index,
                                              context.memory->basic_unit.block.max_of_each);
    ++context.memory->basic_unit.segment.current;
    ++context.val_segments.current;
    printf("%s %s: pid: %d create new segment #%u idle = %u.\n", __FILE__, __func__, getpid(),
           context.val_segments.current, context.memory->idle_list.block_current);
    return res;
}

int shm_allocator::open_val_segment(context &context, const config &config) {
    for (uint32_t index = context.val_segments.current; index < context.memory->basic_unit.segment.current; ++index) {
        int res = init_val_segment(config.memory_type, config.file, context.val_segments.items[index], index,
                                   context.memory->basic_unit.segment.size, context.enable_create);
        if (res != 0) {
            printf("%s %s: pid: %d init_val_segment() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
        ++context.val_segments.current;
    }
    return 0;
}

int shm_allocator::remove_all(uint32_t type, const char *file, ht_segment &ht_segment, val_segments &val_segments,
                              bool create) {
    int res;
    res = shm_memory::remove(type, file, ht_segment.item.id, ht_segment.item.key);
    for (uint32_t index = 0; index < val_segments.current; ++index) {
        mem_segment *val_segment = val_segments.items + index;
        res = init_val_segment(type, file, *val_segment, index, val_segment->size, create);
        if (res != 0) {
            printf("%s %s: pid: %d init_val_segment() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
        int r = shm_memory::remove(type, file, val_segment->id, val_segment->key);
        if (r != 0) {
            res = r;
            printf("%s %s: pid: %d remove() failed.\n", __FILE__, __func__, getpid());
        }
    }
    return res;
}

hash_entry *shm_allocator::alloc_hash_entry(context &context, const config &config, const key_info &key_info,
                                            const value_info &value_info) {
    hash_entry *new_entry;
    uint32_t total = SHM_MEM_ALIGN_BYTE(key_info.length) + SHM_MEM_ALIGN_BYTE(value_info.length);
    uint32_t rest_of_each_block = context.memory->basic_unit.block.size - (int32_t)sizeof(block_entry);
    uint32_t block_used = (total + rest_of_each_block - 1) / rest_of_each_block;
    new_entry = do_alloc_hash_entry(context, block_used, key_info, value_info);
    if (new_entry != nullptr) {
        return new_entry;
    }
    if (context.memory->basic_unit.segment.current < context.memory->basic_unit.segment.max) {
        if (create_val_segment(context, config) == 0) {
            new_entry = do_alloc_hash_entry(context, block_used, key_info, value_info);
        } else {
            printf("%s %s: pid: %d create_val_segment() failed.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
    } else {
        if (context.memory->busy_list.entry_current > 0) {
            if (shm_hashtable::ht_recycle(context, config, block_used, false) == 0) {
                new_entry = do_alloc_hash_entry(context, block_used, key_info, value_info);
            } else {
                printf("%s %s: pid: %d ht_recycle() failed.\n", __FILE__, __func__, getpid());
                if (shm_hashtable::ht_recycle(context, config, block_used, true) == 0) {
                    new_entry = do_alloc_hash_entry(context, block_used, key_info, value_info);
                } else {
                    printf("%s %s: pid: %d ht_recycle(force) failed -> ht_clear()!\n", __FILE__, __func__, getpid());
                    shm_hashtable::ht_clear(context, context.memory->global_stats);
                    new_entry = do_alloc_hash_entry(context, block_used, key_info, value_info);
                }
            }
        } else {
            printf("%s %s: pid: %d recycle forbidden or recycle list empty.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
    }
    if (new_entry == nullptr) {
        printf("%s %s: pid: %d do_alloc_hash_entry() failed x2.\n", __FILE__, __func__, getpid());
    }
    return new_entry;
}

hash_entry *shm_allocator::do_alloc_hash_entry(context &context, uint32_t required_block, const key_info &key_info,
                                               const value_info &value_info) {
    if (required_block > context.memory->idle_list.block_current) {
        return nullptr;
    }
    hash_entry *new_entry = (hash_entry *)(context.ht_segment.item.base + context.memory->entry_queue.offset_2base) +
                            context.memory->entry_queue.tail;
    context.memory->entry_queue.tail_forward();
    // set new hash entry's attributes: 'block_used' and 'first_addr' during allocating
    if (!context.memory->idle_list.alloc_hash_entry_block(context.val_segments, *new_entry, required_block)) {
        printf("%s %s: pid: %d alloc_hash_entry_block() failed.\n", __FILE__, __func__, getpid());
        shm_hashtable::ht_clear(context, context.memory->global_stats);
        return nullptr;
    }
    uint32_t write_start{}, write_end{};
    if (context.enable_stats) {
        write_start = local_stats::get_cpu_cycle();
    }
    // set new hash entry's other attributes and key/value data
    new_entry->write_data(context.val_segments, key_info, value_info, context.memory->basic_unit.block.size);
    if (context.enable_stats) {
        write_end = local_stats::get_cpu_cycle();
        context.local_stats.w_data.all_cost += write_end - write_start;
        ++context.local_stats.w_data.call_count;
        context.local_stats.w_data.max_cost = std::max(write_end - write_start, context.local_stats.w_data.max_cost);
    }
    return new_entry;
}

int shm_allocator::free_hash_entry(context &context, int64_t removed_offset) {
    auto removed_entry = (hash_entry *)(context.ht_segment.item.base + removed_offset);
    if (!context.memory->idle_list.free_hash_entry_block(context.val_segments, *removed_entry)) {
        printf("%s %s: pid: %d free_hash_entry() failed.\n", __FILE__, __func__, getpid());
        return -1;
    }
    hash_entry *first_entry = (hash_entry *)(context.ht_segment.item.base + context.memory->entry_queue.offset_2base) +
                              context.memory->entry_queue.head;
    if (first_entry != removed_entry) {
        int64_t prev_lru_offset = first_entry->lru_prev;
        auto *prev_lru_entry = (hash_entry *)(context.ht_segment.item.base + prev_lru_offset);
        int64_t next_lru_offset = first_entry->lru_next;
        auto *next_lru_entry = (hash_entry *)(context.ht_segment.item.base + next_lru_offset);
        prev_lru_entry->lru_next = removed_offset;
        next_lru_entry->lru_prev = removed_offset;
        char *key_data = context.val_segments.items[first_entry->first_addr.index].base +
                         (uint32_t)first_entry->first_addr.number * context.memory->basic_unit.block.size +
                         sizeof(block_entry);
        key_info temp_key_info(first_entry->key_len, key_data);
        uint32_t ht_index = shm_hashtable::bucket_index(context, temp_key_info);
        int64_t old_offset = context.memory->hashtable.bucket[ht_index];
        bool found = false;
        hash_entry *prev_entry = nullptr;
        hash_entry *old_entry = nullptr;
        while (old_offset > 0) {
            old_entry = (hash_entry *)(context.ht_segment.item.base + old_offset);
            if (shm_hashtable::same_key(context, old_entry, temp_key_info)) {
                found = true;
                break;
            }
            old_offset = old_entry->hash_next;
            prev_entry = old_entry;
        }
        if (!found) {
            printf("%s %s: pid: %d free_hash_entry() failed, clear hashtable...\n", __FILE__, __func__, getpid());
            shm_hashtable::ht_clear(context, context.memory->global_stats);
        }
        if (prev_entry != nullptr) {
            prev_entry->hash_next = removed_offset;
        } else {
            context.memory->hashtable.bucket[ht_index] = removed_offset;
        }
        removed_entry->update(*first_entry);
    }
    context.memory->entry_queue.head_forward();
    --context.memory->hashtable.inserted;
    --context.memory->busy_list.entry_current;
    return 0;
}
