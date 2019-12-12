#include "shm_cache.h"
#include "shm_allocator.h"
#include "shm_configure.h"
#include "shm_hashtable.h"
#include "shm_lock.h"
#include "shm_memory.h"
#include <cerrno>
#include <unistd.h>

shm_cache::shm_cache() { reset(); }

shm_cache::~shm_cache() {
    if (m_context.val_segments.items != nullptr) {
        free(m_context.val_segments.items);
        m_context.val_segments.items = nullptr;
    }
}

int shm_cache::init(const char *file, bool create, bool check) {
    int res;
    if ((res = load_config(file)) != 0) {
        printf("%s %s: pid: %d load_config() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    if ((res = do_init(create, check)) != 0) {
        printf("%s %s: pid: %d do_init() failed, try to release() and init() again.\n", __FILE__, __func__, getpid());
        return res;
    }
    printf("%s %s: pid: %d  shm_cache object initialized successfully.\n"
           "segment = %uMB current = %u max = %u block = %uKB max of each = %u.\n",
           __FILE__, __func__, getpid(), m_context.memory->basic_unit.segment.size / 1024 / 1024,
           m_context.memory->basic_unit.segment.current, m_context.memory->basic_unit.segment.max,
           m_context.memory->basic_unit.block.size / 1024, m_context.memory->basic_unit.block.max_of_each);
    return res;
}

int shm_cache::set_ex(char *key, char *value, int ttl, int op) {
    if (key == nullptr || value == nullptr || ttl < 0 || op < 0) {
        printf("%s %s: pid: %d invalid parameter.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    key_info key_info(key);
    value_info value_info(value, (uint32_t)op, (uint32_t)ttl);
    return set(key_info, value_info);
}

int shm_cache::set_ttl_ex(char *key, int ttl) {
    if (key == nullptr || ttl < 0) {
        printf("%s %s: pid: %d invalid parameter.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    key_info key_info(key);
    return set_ttl(key_info, (uint32_t)ttl);
}

int shm_cache::set_expires_ex(char *key, int expires) {
    if (key == nullptr || expires < 0) {
        printf("%s %s: pid: %d invalid parameter.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    key_info key_info(key);
    return set_expires(key_info, (uint32_t)expires);
}

int shm_cache::get_ex(char *key, char *value, int lru) {
    if (key == nullptr || value == nullptr || lru < 0) {
        printf("%s %s: pid: %d invalid parameter.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    key_info key_info(key);
    value_info value_info(value, 0, 0);
    return get(key_info, value_info, (uint32_t)lru);
}

int shm_cache::del_ex(char *key) {
    if (key == nullptr) {
        printf("%s %s: pid: %d invalid parameter.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    key_info key_info(key);
    return del(key_info);
}

int shm_cache::set(const key_info &key_info, const value_info &value_info) {
    int res;
    uint32_t start{}, end{}, lock_start{}, lock_end{};
    if (m_context.enable_stats) {
        start = local_stats::get_cpu_cycle();
    }
    if (key_info.length > m_config.max_key_size) {
        printf("%s %s: pid: %d invalid key size.\n", __FILE__, __func__, getpid());
        return ENAMETOOLONG;
    }
    if (value_info.length > m_config.max_value_size) {
        printf("%s %s: pid: %d invalid value size.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    if (m_context.enable_stats) {
        lock_start = local_stats::get_cpu_cycle();
    }
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    if (m_context.enable_stats) {
        lock_end = local_stats::get_cpu_cycle();
        m_context.local_stats.w_lock.all_cost += lock_end - lock_start;
        ++m_context.local_stats.w_lock.call_count;
        m_context.local_stats.w_lock.max_cost = std::max(lock_end - lock_start, m_context.local_stats.w_lock.max_cost);
    }
    check_consistence();
    ++m_context.memory->global_stats.set.total;
    res = shm_hashtable::ht_set(m_context, m_config, key_info, value_info);
    if (res == 0) {
        ++m_context.memory->global_stats.set.success;
    }
    shm_lock::write_unlock(m_context);
    if (m_context.enable_stats) {
        end = local_stats::get_cpu_cycle();
        m_context.local_stats.set.all_cost += end - start;
        ++m_context.local_stats.set.call_count;
        if (res == 0) {
            ++m_context.local_stats.set.all_ok_cost += end - start;
            ++m_context.local_stats.set.call_ok_count;
            m_context.local_stats.set.max_ok_cost = std::max(end - start, m_context.local_stats.set.max_ok_cost);
        }
    }
    return res;
}

int shm_cache::set_ttl(const key_info &key_info, uint32_t ttl) {
    int res;
    if (key_info.length > m_config.max_key_size) {
        printf("%s %s: pid: %d invalid key size.\n", __FILE__, __func__, getpid());
        return ENAMETOOLONG;
    }
    if (ttl <= 0) {
        printf("%s %s: pid: %d invalid ttl.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    check_consistence();
    res = shm_hashtable::ht_set_expires(m_context, key_info, ttl == 0 ? 0 : ttl + (uint32_t)time(nullptr));
    shm_lock::write_unlock(m_context);
    return res;
}

int shm_cache::set_expires(const key_info &key_info, uint32_t expires) {
    int res;
    if (key_info.length > m_config.max_key_size) {
        printf("%s %s: pid: %d invalid key size.\n", __FILE__, __func__, getpid());
        return ENAMETOOLONG;
    }
    if (expires <= 0 || (expires > 0 && expires < time(nullptr))) {
        printf("%s %s: pid: %d invalid expires.\n", __FILE__, __func__, getpid());
        return EINVAL;
    }
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    check_consistence();
    res = shm_hashtable::ht_set_expires(m_context, key_info, expires);
    shm_lock::write_unlock(m_context);
    return res;
}

int shm_cache::get(const key_info &key_info, value_info &value_info, uint32_t lru) {
    int res;
    uint32_t start{}, end{}, lock_start{}, lock_end{};
    if (m_context.enable_stats) {
        start = local_stats::get_cpu_cycle();
    }
    if (key_info.length > m_config.max_key_size) {
        printf("%s %s: pid: %d invalid key size.\n", __FILE__, __func__, getpid());
        return ENAMETOOLONG;
    }
    if (m_context.enable_stats) {
        lock_start = local_stats::get_cpu_cycle();
    }
    if ((res = shm_lock::read_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    if (m_context.enable_stats) {
        lock_end = local_stats::get_cpu_cycle();
        m_context.local_stats.r_lock.all_cost += lock_end - lock_start;
        ++m_context.local_stats.r_lock.call_count;
        m_context.local_stats.r_lock.max_cost = std::max(lock_end - lock_start, m_context.local_stats.r_lock.max_cost);
    }
    check_consistence();
    ++m_context.memory->global_stats.get.total;
    res = shm_hashtable::ht_get(m_context, key_info, value_info, lru);
    if (res == 0) {
        ++m_context.memory->global_stats.get.success;
        m_context.memory->global_stats.get_bytes += value_info.length;
    }
    shm_lock::read_unlock(m_context);
    if (m_context.enable_stats) {
        end = local_stats::get_cpu_cycle();
        m_context.local_stats.get.all_cost += end - start;
        ++m_context.local_stats.get.call_count;
        m_context.local_stats.get.max_ok_cost = std::max(end - start, m_context.local_stats.get.max_ok_cost);
        if (res == 0) {
            ++m_context.local_stats.get.all_ok_cost += end - start;
            ++m_context.local_stats.get.call_ok_count;
            m_context.local_stats.get.max_ok_cost = std::max(end - start, m_context.local_stats.get.max_ok_cost);
        }
    }
    return res;
}

int shm_cache::del(const key_info &key_info) {
    int res;
    uint32_t start{}, end{};
    if (m_context.enable_stats) {
        start = local_stats::get_cpu_cycle();
    }
    if (key_info.length > m_config.max_key_size) {
        printf("%s %s: pid: %d invalid key size.\n", __FILE__, __func__, getpid());
        return ENAMETOOLONG;
    }
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    check_consistence();
    ++m_context.memory->global_stats.del.total;
    res = shm_hashtable::ht_del(m_context, key_info, false);
    if (res == 0) {
        ++m_context.memory->global_stats.del.success;
    }
    shm_lock::write_unlock(m_context);
    if (m_context.enable_stats) {
        end = local_stats::get_cpu_cycle();
        m_context.local_stats.del.all_cost += end - start;
        ++m_context.local_stats.del.call_count;
        m_context.local_stats.del.max_ok_cost = std::max(end - start, m_context.local_stats.del.max_ok_cost);
        if (res == 0) {
            ++m_context.local_stats.del.all_ok_cost += end - start;
            ++m_context.local_stats.del.call_ok_count;
            m_context.local_stats.del.max_ok_cost = std::max(end - start, m_context.local_stats.del.max_ok_cost);
        }
    }
    return res;
}

int shm_cache::destroy() {
    int res = 0;
    for (uint32_t index = 0; index < m_context.val_segments.current; ++index) {
        if (m_context.val_segments.items[index].base != nullptr) {
            res += shm_memory::unmap(m_config.memory_type, m_context.val_segments.items[index].base,
                                     m_context.val_segments.items[index].size);
            m_context.val_segments.items[index].base = nullptr;
        }
    }
    if (m_context.ht_segment.item.base != nullptr) {
        res += shm_memory::unmap(m_config.memory_type, m_context.ht_segment.item.base, m_context.ht_segment.item.size);
        m_context.ht_segment.item.base = nullptr;
    }
    return res;
}

int shm_cache::remove() {
    int res;
    if ((res = shm_lock::file_lock(m_context, m_config)) != 0) {
        return res;
    }
    check_consistence();
    if ((res = shm_allocator::remove_all(m_config.memory_type, m_config.file, m_context.ht_segment,
                                         m_context.val_segments, m_context.enable_create)) != 0) {
        printf("%s %s: pid: %d remove_segment() failed.\n", __FILE__, __func__, getpid());
    }
    shm_lock::file_unlock(m_context);
    return res;
}

int shm_cache::clear_hashtable() {
    int res;
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    check_consistence();
    res = shm_hashtable::ht_clear(m_context, m_context.memory->global_stats);
    shm_lock::write_unlock(m_context);
    return res;
}

time_t shm_cache::get_last_ht_clear_time() const { return m_context.memory->global_stats.last_clear_time; }

stats_output shm_cache::get_global_stats() {
    stats_output res;
    if (shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats) != 0) {
        return res;
    }
    time_t current_time = time(nullptr);
    res.global_stats = m_context.memory->global_stats;
    time_t time_delta = current_time - res.global_stats.last.time;
    res.performance.seconds = time_delta;
    m_context.memory->global_stats.last.time = current_time;

    uint32_t total_delta;
    uint32_t success_delta;
    uint32_t survive_delta;
    uint32_t eliminate_delta;
    uint32_t save_delta;
    uint32_t lru_delta;

    total_delta = res.global_stats.get.total - m_context.memory->global_stats.last.get.total;
    success_delta = res.global_stats.get.success - m_context.memory->global_stats.last.get.success;
    survive_delta = res.global_stats.survive_duration - m_context.memory->global_stats.last.survive_duration;
    eliminate_delta = res.global_stats.eliminate_count - m_context.memory->global_stats.last.eliminate_count;
    save_delta = res.global_stats.get_bytes - m_context.memory->global_stats.last.get_bytes;
    lru_delta = res.global_stats.lru_count - m_context.memory->global_stats.last.lru_times;

    res.performance.get_ratio = total_delta > 0 ? (double)success_delta / (double)total_delta : -1.0;
    res.performance.get_qps = time_delta > 0 ? (double)total_delta / (double)time_delta : (double)total_delta;
    res.performance.survive = eliminate_delta > 0 ? (double)survive_delta / (double)eliminate_delta : -1.0;
    res.performance.save_bytes = save_delta;
    res.performance.lru_times = lru_delta;

    m_context.memory->global_stats.last.get.total = res.global_stats.get.total;
    m_context.memory->global_stats.last.get.success = res.global_stats.get.success;
    m_context.memory->global_stats.last.survive_duration = res.global_stats.survive_duration;
    m_context.memory->global_stats.last.eliminate_count = res.global_stats.eliminate_count;
    m_context.memory->global_stats.last.get_bytes = res.global_stats.get_bytes;
    m_context.memory->global_stats.last.lru_times = res.global_stats.lru_count;

    shm_lock::write_unlock(m_context);
    return res;
}

int shm_cache::clear_global_stats() {
    int res;
    if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
        return res;
    }
    m_context.memory->global_stats.reset();
    shm_lock::write_unlock(m_context);
    return 0;
}

void shm_cache::reset() {
    m_config.reset();
    m_context.reset();
}

int shm_cache::load_config(const char *file) {
    if (file == nullptr) {
        printf("%s %s: pid: %d file = nullptr.\n", __FILE__, __func__, getpid());
        return -1;
    }
    shm_configure conf(file);
    std::string str;
    str = conf.get_string_value("filename");
    if (str.empty()) {
        return -1;
    } else {
        memset(&m_config.file, 0, SHM_MAX_PATH_SIZE);
        memcpy(&m_config.file, str.c_str(), str.length());
    }
    str = conf.get_string_value("logdir");
    if (str.empty()) {
        return -1;
    } else {
        memset(&m_config.dir, 0, SHM_MAX_PATH_SIZE);
        memcpy(&m_config.dir, str.c_str(), str.length());
    }
    str = conf.get_string_value("type");
    if (str.empty()) {
        return -1;
    } else {
        m_config.memory_type = str != "shm" ? SHM_MEM_TYPE_MMAP : SHM_MEM_TYPE_SHM;
    }
    str = conf.get_string_value("recycle_valid");
    if (str.empty()) {
        return -1;
    } else {
        m_config.recycle_valid = str != "false";
    }
    int64_t integer;
    integer = conf.get_integer_value("max_mem_mb");
    if (integer < 0) {
        return -1;
    } else {
        m_config.max_mem_mb = (uint32_t)integer;
    }
    integer = conf.get_integer_value("min_mem_mb");
    if (integer < 0) {
        return -1;
    } else {
        m_config.min_mem_mb = (uint32_t)integer;
    }
    integer = conf.get_integer_value("segment_size");
    if (integer < 0) {
        return -1;
    } else {
        m_config.segment_size = (uint32_t)integer;
    }
    integer = conf.get_integer_value("block_size");
    if (integer < 0) {
        return -1;
    } else {
        m_config.block_size = (uint32_t)integer;
    }
    integer = conf.get_integer_value("max_key_count");
    if (integer < 0) {
        return -1;
    } else {
        m_config.max_key_count = (uint32_t)integer;
    }
    integer = conf.get_integer_value("max_key_size");
    if (integer < 0) {
        return -1;
    } else {
        m_config.max_key_size = (uint32_t)integer;
    }
    integer = conf.get_integer_value("max_value_size");
    if (integer < 0) {
        return -1;
    } else {
        m_config.max_value_size = (uint32_t)integer;
    }
    integer = conf.get_integer_value("try_r_lk_interval");
    if (integer < 0) {
        return -1;
    } else {
        m_config.try_r_lk_interval = (uint32_t)integer;
    }
    integer = conf.get_integer_value("try_w_lk_interval");
    if (integer < 0) {
        return -1;
    } else {
        m_config.try_w_lk_interval = (uint32_t)integer;
    }
    integer = conf.get_integer_value("detect_r_dl_ticks");
    if (integer < 0) {
        return -1;
    } else {
        m_config.detect_r_dl_ticks = (uint32_t)integer;
    }
    integer = conf.get_integer_value("detect_w_dl_ticks");
    if (integer < 0) {
        return -1;
    } else {
        m_config.detect_w_dl_ticks = (uint32_t)integer;
    }
    return 0;
}

int shm_cache::do_init(const bool create, const bool check) {
    int res;
    m_context.lock_fd = -1;
    m_context.enable_create = create;
    m_context.enable_stats = false;

    basic_unit basic_unit;
    hashtable hashtable;
    uint32_t total;
    uint32_t offset_2base;
    get_unit_and_ht(basic_unit, hashtable, total, offset_2base);
    bool exists = shm_memory::exists(m_config.memory_type, m_config.file, SHM_HT_SEGMENT_ID);
    if ((res = shm_allocator::init_ht_segment(m_config.memory_type, m_config.file, m_context.ht_segment.item,
                                              SHM_HT_SEGMENT_ID, total, m_context.enable_create)) != 0) {
        printf("%s %s: pid: %d init_ht_segment() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    uint32_t bytes = (uint32_t)sizeof(mem_segment) * basic_unit.segment.max;
    m_context.val_segments.items = (mem_segment *)malloc(bytes);
    if (m_context.val_segments.items == nullptr) {
        printf("%s %s: pid: %d malloc() failed.\n", __FILE__, __func__, getpid());
        return ENOMEM;
    }
    memset(m_context.val_segments.items, 0, bytes);
    m_context.memory = (memory_info *)m_context.ht_segment.item.base;
    if (exists && check) {
        printf("%s %s: pid: %d ht_segment exists.\n", __FILE__, __func__, getpid());
        res = check_ht_segment(basic_unit, offset_2base);
        if (res != 0) {
            printf("%s %s: pid: %d check_ht_segment() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
    }
    if (create) {
        if (m_context.memory->status == SHM_STATUS_INIT) {
            res = do_lock_init(basic_unit, hashtable, offset_2base);
            if (!(res == 0 || res == -EEXIST)) {
                printf("%s %s: pid: %d do_lock_init() failed.\n", __FILE__, __func__, getpid());
                return res;
            }
        }
        res = shm_allocator::open_val_segment(m_context, m_config);
        if (res != 0) {
            printf("%s %s: pid: %d open_val_segment() failed.\n", __FILE__, __func__, getpid());
        }
        if (res == 0 && m_context.memory->basic_unit.segment.current < m_context.memory->basic_unit.segment.max) {
            if ((res = shm_lock::write_lock(m_context, m_config, m_context.memory->global_stats)) != 0) {
                printf("%s %s: pid: %d w_lock() failed.\n", __FILE__, __func__, getpid());
                return res;
            }
            while (m_context.ht_segment.item.size +
                       (uint64_t)m_context.val_segments.current * (uint64_t)m_context.memory->basic_unit.segment.size <
                   (uint64_t)m_config.min_mem_mb * 1024 * 1024) {
                if ((res = shm_allocator::create_val_segment(m_context, m_config)) != 0) {
                    break;
                }
                if (m_context.memory->basic_unit.segment.current >= m_context.memory->basic_unit.segment.max) {
                    break;
                }
            }
            shm_lock::write_unlock(m_context);
        }
    }
    return res;
}

int shm_cache::do_lock_init(const basic_unit &basic_unit, const hashtable &hashtable, uint32_t &offset_2base) {
    int res;
    if ((res = shm_lock::file_lock(m_context, m_config)) != 0) {
        return res;
    }
    if (m_context.memory->status == SHM_STATUS_NORMAL) {
        printf("%s %s: pid: %d status = SHM_STATUS_NORMAL.\n", __FILE__, __func__, getpid());
        res = -EEXIST;
    } else {
        m_context.memory->basic_unit = basic_unit;
        m_context.memory->hashtable = hashtable;
        m_context.memory->busy_list.entry_size = sizeof(hash_entry);
        m_context.memory->busy_list.offset_f2base =
            (char *)&m_context.memory->busy_list.fake_entry - m_context.ht_segment.item.base;
        m_context.memory->busy_list.reset();
        m_context.memory->idle_list.block_size = basic_unit.block.size;
        m_context.memory->idle_list.offset_f2base =
            (char *)&m_context.memory->idle_list.fake_block - m_context.ht_segment.item.base;
        m_context.memory->idle_list.reset(m_context.val_segments, m_context.memory->basic_unit.block.max_of_each);
        m_context.memory->entry_queue.capacity = m_config.max_key_count;
        m_context.memory->entry_queue.offset_2base = offset_2base;
        m_context.memory->entry_queue.reset();
        if ((res = shm_lock::lock_init(m_context)) != 0) {
            printf("%s %s: pid: %d lock_init() failed.\n", __FILE__, __func__, getpid());
        } else {
            if ((res = shm_allocator::create_val_segment(m_context, m_config)) != 0) {
                printf("%s %s: pid: %d create_val_segment() failed.\n", __FILE__, __func__, getpid());
            } else {
                m_context.memory->global_stats.reset();
                m_context.memory->init_time = time(nullptr);
                m_context.memory->size = (int32_t)sizeof(memory_info);
                m_context.memory->max_key_count = m_config.max_key_count;
                m_context.memory->status = SHM_STATUS_NORMAL;
            }
        }
    }
    shm_lock::file_unlock(m_context);
    return res;
}

int shm_cache::check_ht_segment(const basic_unit &basic_unit, uint32_t &offset_2base) const {
    if (m_context.memory->size != (int32_t)sizeof(memory_info)) {
        return EINVAL;
    }
    if (m_context.memory->status != SHM_STATUS_NORMAL) {
        return EINVAL;
    }
    if (m_context.memory->max_key_count != m_config.max_key_count) {
        return EINVAL;
    }
    if (m_context.memory->basic_unit.segment.size != basic_unit.segment.size ||
        m_context.memory->basic_unit.segment.max != basic_unit.segment.max) {
        return EINVAL;
    }
    if (m_context.memory->basic_unit.block.size != basic_unit.block.size ||
        m_context.memory->basic_unit.block.max_of_each != basic_unit.block.max_of_each) {
        return EINVAL;
    }
    if (m_context.memory->busy_list.entry_size != sizeof(hash_entry) ||
        m_context.memory->idle_list.block_size != basic_unit.block.size) {
        return EINVAL;
    }
    if (m_context.memory->busy_list.offset_f2base !=
            (char *)&m_context.memory->busy_list.fake_entry - m_context.ht_segment.item.base ||
        m_context.memory->idle_list.offset_f2base !=
            (char *)&m_context.memory->idle_list.fake_block - m_context.ht_segment.item.base ||
        m_context.memory->entry_queue.offset_2base != offset_2base) {
        return EINVAL;
    }
    return 0;
}

void shm_cache::get_unit_and_ht(basic_unit &basic_unit, hashtable &hashtable, uint32_t &total_size,
                                uint32_t &offset_2base) {
    total_size = 0;
    calc_basic_uint(basic_unit, (uint64_t)m_config.max_mem_mb * 1024 * 1024);
    hashtable.inserted = 0;
    hashtable.capacity = shm_hashtable::get_capacity(m_config.max_key_count);
    total_size += (uint32_t)sizeof(memory_info);
    total_size += (uint32_t)sizeof(int64_t) * (hashtable.capacity);
    offset_2base = total_size;
    total_size += (uint32_t)sizeof(hash_entry) * (m_config.max_key_count);
    calc_basic_uint(basic_unit, (uint64_t)m_config.max_mem_mb * 1024 * 1024 - total_size);
}

void shm_cache::calc_basic_uint(basic_unit &basic_uint, uint64_t max_memory) {
    auto page_size = (uint32_t)getpagesize();
    basic_uint.segment.size = SHM_MEM_ALIGN(m_config.segment_size, page_size);
    basic_uint.block.size = SHM_MEM_ALIGN(m_config.block_size, page_size);
    if (basic_uint.segment.size % basic_uint.block.size != 0) {
        printf("%s %s: pid: %d segment.size mod block.size != 0, use default.\n", __FILE__, __func__, getpid());
        basic_uint.segment.size = SHM_SEGMENT_SIZE;
        basic_uint.block.size = SHM_BLOCK_SIZE;
    }
    if (basic_uint.block.size - (int32_t)sizeof(block_entry) < m_config.max_key_size) {
        printf("%s %s: pid: %d basic_uint.block.size two small, use default.\n", __FILE__, __func__, getpid());
        basic_uint.segment.size = SHM_SEGMENT_SIZE;
        basic_uint.block.size = SHM_BLOCK_SIZE;
    }
    basic_uint.block.max_of_each = basic_uint.segment.size / basic_uint.block.size;
    basic_uint.segment.max = (uint32_t)(max_memory / basic_uint.segment.size);
    if (basic_uint.segment.max == 0) {
        basic_uint.segment.max = 1;
    }
    basic_uint.segment.current = 0;
}

void shm_cache::start_local_observe() {
    if (!m_context.enable_stats) {
        m_context.enable_stats = true;
        m_context.local_stats.start();
    }
}

std::string shm_cache::stop_local_observe(bool write2file, bool clear) {
    std::string res;
    if (m_context.enable_stats) {
        m_context.enable_stats = false;
        m_context.local_stats.stop();
        res = m_context.local_stats.serialize(write2file, m_config.dir);
        if (clear) {
            m_context.local_stats.reset();
        }
    }
    return res;
}

void shm_cache::clear_local_stats() {
    if (m_context.enable_stats) {
        m_context.local_stats.reset();
    }
}

int shm_cache::check_consistence() {
    if (shm_allocator::open_val_segment(m_context, m_config) != 0) {
        printf("%s %s: pid: %d open_val_segment()failed.\n", __FILE__, __func__, getpid());
        return -1;
    }
    return 0;
}
