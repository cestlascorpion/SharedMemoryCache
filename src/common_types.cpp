#include "common_types.h"
#include "shm_serialization.h"
#include <unistd.h>

using std::string;
using std::to_string;

void local_stats::show() {
    printf("---------------------------------------------------------------------------------------------------\n");
    if (start_cycle < end_cycle) {
        printf("- pid: %d running time = %.4fs\n", getpid(),
               (double)(end_cycle - start_cycle) / 1000.0 / 1000.0 / (double)cpu_freq);
    }
    printf("---------------------------------------------------------------------------------------------------\n");
    if (set.call_count > 0 && set.call_ok_count > 0) {
        printf("- pid: %d set info: call %u \tsucceed %.4f\tavg_time %.4f\t max_time = %.4f\n", getpid(),
               set.call_count, (double)set.call_ok_count / (double)set.call_count,
               (double)set.all_ok_cost / (double)set.call_ok_count / cpu_freq, (double)set.max_ok_cost / cpu_freq);
        printf("- pid: %d set warn: fail %u \tavg_time %.4f\n", getpid(), set.call_count - set.call_ok_count,
               set.call_count > set.call_ok_count
                   ? (double)(set.all_cost - set.all_ok_cost) / (double)(set.call_count - set.call_ok_count)
                   : 0.0);
    }
    if (w_data.call_count > 0) {
        printf("- pid: %d w_data info: call %u \tavg_time %.4f\tmax_time = %.4f\n", getpid(), w_data.call_count,
               (double)w_data.all_cost / (double)w_data.call_count / cpu_freq, (double)w_data.max_cost / cpu_freq);
    }
    if (w_lock.call_count > 0) {
        printf("- pid: %d w_lock info: call %u \tavg_time %.4f\tmax_time = %.4f\n", getpid(), w_lock.call_count,
               (double)w_lock.all_cost / (double)w_lock.call_count / cpu_freq, (double)w_lock.max_cost / cpu_freq);
    }
    printf("---------------------------------------------------------------------------------------------------\n");
    if (get.call_count > 0 && get.call_ok_count > 0) {
        printf("- pid: %d get info: call %u \tsucceed %.4f\tavg_time %.4f\tmax_time = %.4f\n", getpid(), get.call_count,
               (double)get.call_ok_count / (double)get.call_count,
               (double)get.all_ok_cost / (double)get.call_ok_count / cpu_freq, (double)get.max_ok_cost / cpu_freq);
        printf("- pid: %d get warn: fail %u \tavg_time %.4f\n", getpid(), get.call_count - get.call_ok_count,
               get.call_count > get.call_ok_count
                   ? (double)(get.all_cost - get.all_ok_cost) / (double)(get.call_count - get.call_ok_count)
                   : 0.0);
    }
    if (r_data.call_count > 0) {
        printf("- pid: %d r_data info: call %u \tavg_time %.4f\tmax_time = %.4f\n", getpid(), r_data.call_count,
               (double)r_data.all_cost / (double)r_data.call_count / cpu_freq, (double)r_data.max_cost / cpu_freq);
    }
    if (r_lock.call_count > 0) {
        printf("- pid: %d r_lock info: call %u \tavg_time %.4f\tmax_time = %.4f\n", getpid(), r_lock.call_count,
               (double)r_lock.all_cost / (double)r_lock.call_count / cpu_freq, (double)r_lock.max_cost / cpu_freq);
    }
    printf("---------------------------------------------------------------------------------------------------\n");
    if (del.call_count > 0 && del.call_ok_count > 0) {
        printf("- pid: %d del info: call %u \tsucceed %.4f\tavg_time %.4f\tmax_time = %.4f\n", getpid(), del.call_count,
               (double)del.call_ok_count / (double)del.call_count,
               (double)del.all_ok_cost / (double)del.call_ok_count / cpu_freq, (double)del.max_ok_cost / cpu_freq);
        printf("- pid: %d del warn: fail %u \tavg_time %.4f\n", getpid(), del.call_count - del.call_ok_count,
               del.call_count > del.call_ok_count
                   ? (double)(del.all_cost - del.all_ok_cost) / (double)(del.call_count - del.call_ok_count)
                   : 0.0);
    }
    printf("---------------------------------------------------------------------------------------------------\n");
}

string local_stats::serialize(const bool write, const char *dir) {
    shm_serialization helper;
    string lval, rval;
    if (start_cycle < end_cycle) {
        lval = "running_time";
        rval = to_string((double)(end_cycle - start_cycle) / 1000.0 / 1000.0 / (double)cpu_freq);
        helper.put_data(lval, rval);
    }
    if (set.call_count > 0 && set.call_ok_count > 0) {
        lval = "set_call";
        rval = to_string(set.call_count);
        helper.put_data(lval, rval);
        lval = "set_ok_call";
        rval = to_string(set.call_ok_count);
        helper.put_data(lval, rval);
        lval = "set_succeed";
        rval = to_string((double)set.call_ok_count / (double)set.call_count);
        helper.put_data(lval, rval);
        lval = "set_ok_avg_time";
        rval = to_string((double)set.all_ok_cost / (double)set.call_ok_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "set_ok_max_time";
        rval = to_string((double)set.max_ok_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (get.call_count > 0 && get.call_ok_count > 0) {
        lval = "get_call";
        rval = to_string(get.call_count);
        helper.put_data(lval, rval);
        lval = "get_ok_call";
        rval = to_string(get.call_ok_count);
        helper.put_data(lval, rval);
        lval = "get_succeed";
        rval = to_string((double)get.call_ok_count / (double)get.call_count);
        helper.put_data(lval, rval);
        lval = "get_ok_avg_time";
        rval = to_string((double)get.all_ok_cost / (double)get.call_ok_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "get_ok_max_time";
        rval = to_string((double)get.max_ok_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (del.call_count > 0 && del.call_ok_count > 0) {
        lval = "del_call";
        rval = to_string(del.call_count);
        helper.put_data(lval, rval);
        lval = "del_ok_call";
        rval = to_string(del.call_ok_count);
        helper.put_data(lval, rval);
        lval = "del_succeed";
        rval = to_string((double)del.call_ok_count / (double)del.call_count);
        helper.put_data(lval, rval);
        lval = "del_ok_avg_time";
        rval = to_string((double)del.all_ok_cost / (double)del.call_ok_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "del_ok_max_time";
        rval = to_string((double)del.max_ok_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (w_data.call_count > 0) {
        lval = "write_data_call";
        rval = to_string(w_data.call_count);
        helper.put_data(lval, rval);
        lval = "write_data_avg_time";
        rval = to_string((double)w_data.all_cost / (double)w_data.call_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "write_data_max_time";
        rval = to_string((double)w_data.max_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (r_data.call_count > 0) {
        lval = "read_data_call";
        rval = to_string(r_data.call_count);
        helper.put_data(lval, rval);
        lval = "read_data_avg_time";
        rval = to_string((double)r_data.all_cost / (double)r_data.call_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "read_data_max_time";
        rval = to_string((double)r_data.max_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (w_lock.call_count > 0) {
        lval = "write_lock_call";
        rval = to_string(w_lock.call_count);
        helper.put_data(lval, rval);
        lval = "write_lock_avg_time";
        rval = to_string((double)w_lock.all_cost / (double)w_lock.call_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "write_lock_max_time";
        rval = to_string((double)w_lock.max_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (r_lock.call_count > 0) {
        lval = "read_lock_call";
        rval = to_string(r_lock.call_count);
        helper.put_data(lval, rval);
        lval = "read_lock_avg_time";
        rval = to_string((double)r_lock.all_cost / (double)r_lock.call_count / cpu_freq);
        helper.put_data(lval, rval);
        lval = "read_lock_max_time";
        rval = to_string((double)r_lock.max_cost / cpu_freq);
        helper.put_data(lval, rval);
    }
    if (write) {
        string directory(dir);
        helper.write_data(directory);
    }
    return helper.simple_serialize();
}

void local_stats::init_cpu_info() {
#ifdef __APPLE__
    cpu_freq = 2300; // 2.3GHz of my Mac
    return;
#endif
    FILE *fp;
    char *str;
    const char *cmd;
    double ratio = 1.0;
    str = (char *)malloc(1024);
    if (str == nullptr) {
        return;
    }
    fp = popen("cat /proc/cpuinfo | grep -m 1 \"model name\"", "r");
    char *fget;
    fget = fgets(str, 1024, fp);
    if (fget == nullptr) {
        printf("%s %s: pid: %d fgets() failed.\n", __FILE__, __func__, getpid());
    }
    fclose(fp);
    if (strstr(str, "AMD")) {
        cmd = R"(cat /proc/cpuinfo | grep -m 1 "cpu MHz" | sed -e 's/.*:[^0-9]//')";
    } else {
        cmd = R"(cat /proc/cpuinfo | grep -m 1 "model name" | sed -e "s/^.*@ //g" -e "s/GHz//g")";
        ratio = 1000;
    }
    fp = popen(cmd, "r");
    if (fp == nullptr) {
        printf("get cpu info failed\n");
        free(str);
        return;
    } else {
        fget = fgets(str, 1024, fp);
        if (fget == nullptr) {
            printf("%s %s: pid: %d fgets() failed.\n", __FILE__, __func__, getpid());
        }
        cpu_freq = strtof(str, nullptr) * ratio;
        fclose(fp);
    }
    free(str);
}

uint32_t local_stats::get_cpu_cycle() {
    union cpu_cycle {
        struct t_i32 {
            uint32_t l;
            uint32_t h;
        } i32;
        uint32_t t;
    } c{};
    SHM_RDTSC(c.i32.l, c.i32.h);
    return c.t;
}

void stats_output::show() {
    printf("during %d s: get_ratio = %f, get_qps = %f survive = %fs save = %dMB lru_times = %u \n",
           (int32_t)performance.seconds, performance.get_ratio, performance.get_qps, performance.survive,
           (int32_t)(performance.save_bytes / 1024 / 1024), performance.lru_times);
    printf("key op: get = %u/%u set = %u/%u del = %u/%u\n"
           "r_lock_total = %u r_lock_retry = %u average = %f\n"
           "w_lock_total = %u w_lock_retry = %u average = %f\n",
           global_stats.get.success, global_stats.get.total, global_stats.set.success, global_stats.set.total,
           global_stats.del.success, global_stats.del.total, global_stats.r_lock_total, global_stats.r_lock_retry,
           global_stats.r_lock_retry + global_stats.r_lock_total / (double)global_stats.r_lock_total,
           global_stats.w_lock_total, global_stats.w_lock_retry,
           global_stats.w_lock_retry + global_stats.w_lock_total / (double)global_stats.w_lock_total);
}

string stats_output::serialize() {
    shm_serialization helper;
    string lval, rval;
    lval = "run_time";
    rval = to_string(performance.seconds);
    helper.put_data(lval, rval);
    lval = "get_ratio";
    rval = to_string(performance.get_ratio);
    helper.put_data(lval, rval);
    lval = "get_qps";
    rval = to_string(performance.get_qps);
    helper.put_data(lval, rval);
    lval = "survive";
    rval = to_string(performance.survive);
    helper.put_data(lval, rval);
    lval = "save";
    rval = to_string(performance.save_bytes / 1024 / 1024);
    helper.put_data(lval, rval);
    lval = "lru_times";
    rval = to_string(performance.lru_times);
    helper.put_data(lval, rval);
    lval = "get_total";
    rval = to_string(global_stats.get.total);
    helper.put_data(lval, rval);
    lval = "get_success";
    rval = to_string(global_stats.get.success);
    helper.put_data(lval, rval);
    lval = "set_total";
    rval = to_string(global_stats.set.total);
    helper.put_data(lval, rval);
    lval = "set_success";
    rval = to_string(global_stats.set.success);
    helper.put_data(lval, rval);
    lval = "del_total";
    rval = to_string(global_stats.del.total);
    helper.put_data(lval, rval);
    lval = "del_success";
    rval = to_string(global_stats.del.success);
    helper.put_data(lval, rval);
    lval = "r_lock_total";
    rval = to_string(global_stats.r_lock_total);
    helper.put_data(lval, rval);
    lval = "r_lock_retry";
    rval = to_string(global_stats.r_lock_retry);
    helper.put_data(lval, rval);
    lval = "r_lock_avg";
    rval = to_string((global_stats.r_lock_retry + global_stats.r_lock_total) / (double)global_stats.r_lock_total);
    helper.put_data(lval, rval);
    lval = "w_lock_total";
    rval = to_string(global_stats.w_lock_total);
    helper.put_data(lval, rval);
    lval = "w_lock_retry";
    rval = to_string(global_stats.w_lock_retry);
    helper.put_data(lval, rval);
    lval = "w_lock_avg";
    rval = to_string((global_stats.w_lock_retry + global_stats.w_lock_total) / (double)(global_stats.w_lock_total));
    helper.put_data(lval, rval);
    return helper.simple_serialize();
}

int hash_entry::check_entry(const val_segments &val_segments, const uint32_t block_size) {
    printf("check entry begin\n");
    if (key_len <= 0 || value_len <= 0 || hash_next <= 0 || lru_prev <= 0 || lru_next <= 0 ||
        !first_addr.valid_addr()) {
        printf("invalid entry!\n");
        return -1;
    }
    block_addr what = first_addr;
    for (unsigned i = 0; i < block_used; ++i) {
        if (i == 0) {
            printf("first block #%d <%d, %d>\n", i, what.index, what.number);
        }
        if (i == block_used - 1) {
            printf(" last block #%d <%d, %d>\n", i, what.index, what.number);
        }
        auto *how = (block_entry *)(val_segments.items[what.index].base + (uint32_t)what.number * block_size);
        what = how->next;
        if (!what.valid_addr() && i != block_used - 1) {
            printf("invalid entry!\n");
            return -1;
        }
    }
    printf("check entry passed\n");
    return 0;
}

int idle_list::check_list(const val_segments &val_segments) {
    printf("check idle begin.\n");
    if (block_size <= 0) {
        printf("invalid idle list! block_size <= 0.\n");
        return -1;
    }
    if (block_current <= 0) {
        printf("invalid idle list! block_current < 0.\n");
        return -1;
    }
    if (offset_f2base <= 0) {
        printf("invalid idle list! offset_f2base <= 0.\n");
        return -1;
    }
    if (block_current > 0 && !fake_block.next.valid_addr()) {
        printf("invalid idle list! block_current > 0 && !fake_block.next.valid().\n");
        return -1;
    }
    block_addr what = fake_block.next;
    for (unsigned i = 0; i < block_current; ++i) {
        if (i == 0) {
            printf("first block #%u <%d, %d>\n", i, what.index, what.number);
        }
        if (i == block_current - 1) {
            printf(" last block #%u <%d, %d>\n", i, what.index, what.number);
        }
        auto *how = (block_entry *)(val_segments.items[what.index].base + (uint32_t)what.number * block_size);
        what = how->next;
        if (!what.valid_addr() && i != block_current - 1) {
            printf("invalid idle list! block chain broken.\n");
            return -1;
        }
    }
    printf("check idle list passed.\n");
    return 0;
}

int busy_list::check_list(const ht_segment &ht_segment) {
    printf("check busy list begin.\n");
    if (entry_size <= 0) {
        printf("invalid busy list! entry_size <= 0.\n");
        return -1;
    }
    if (entry_current <= 0) {
        printf("invalid busy list! entry_current < 0.\n");
        return -1;
    }
    if (offset_f2base <= 0) {
        printf("invalid busy list! offset_f2base <= 0.\n");
        return -1;
    }
    if (entry_current == 0 && (fake_entry.lru_next != offset_f2base || fake_entry.lru_prev != offset_f2base)) {
        printf("invalid busy list! entry_current = 0, fake_entry <-x-> fake_entry.\n");
        return -1;
    }
    if (entry_current > 0 && (fake_entry.lru_next == offset_f2base || fake_entry.lru_prev == offset_f2base)) {
        printf("invalid busy list! entry_current > 0, fake_entry <-> fake_entry.\n");
        return -1;
    }
    int64_t forward[entry_current];
    int64_t backward[entry_current];
    int64_t what = fake_entry.lru_next;
    int64_t tahw = fake_entry.lru_prev;
    unsigned count = 0;
    while (what != offset_f2base || tahw != offset_f2base) {
        if (count == 0) {
            printf("first entry: #%u offset = %ld tesoof = %ld \n", count, what, tahw);
        }
        if (count == entry_current - 1) {
            printf(" last entry: #%u offset = %ld tesoof = %ld \n", count, what, tahw);
        }
        forward[count] = what;
        backward[count] = tahw;
        auto *how = (hash_entry *)(ht_segment.item.base + what);
        auto *woh = (hash_entry *)(ht_segment.item.base + tahw);
        what = how->lru_next;
        tahw = woh->lru_prev;
        ++count;
    }
    if (count != entry_current) {
        printf("invalid busy list! lru list broken.\n");
        return -1;
    }
    for (unsigned i = 0; i < entry_current; ++i) {
        if (forward[i] != backward[entry_current - 1 - i]) {
            printf("invalid busy list! lru list broken.\n");
            return -1;
        }
    }
    printf("check busy list passed.\n");
    return 0;
}
