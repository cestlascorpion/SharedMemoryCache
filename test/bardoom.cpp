#include "../src/shm_cache.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <wait.h>

using namespace std;

const uint32_t MAX_KEY_SIZE = 64;
const uint32_t MAX_VALUE_SIZE = 4 * 1024 * 1024;
const uint32_t MIN_VALUE_SIZE = 2 * 1024 * 1024;
const uint32_t MAX_SET_SLEEP_TIME = 2000;
const uint32_t MIN_SET_SLEEP_TIME = 1000;
const uint32_t MAX_GET_SLEEP_TIME = 100;
const uint32_t MIN_GET_SLEEP_TIME = 50;
const uint32_t MAX_DEL_SLEEP_TIME = 100;
const uint32_t MIN_DEL_SLEEP_TIME = 50;
const uint32_t KEY_COUNT = 5000;
const uint32_t SET_TIMES = 500;
const uint32_t GET_TIMES = 1000;
const uint32_t DEL_TIMES = 500;
const uint32_t LRU_K = 1;

using len_vector = vector<uint32_t>;
uint32_t rand_number(uint32_t min, uint32_t max);
uint32_t delta_s(timeval begin, timeval end);
uint32_t delta_ms(timeval begin, timeval end);
uint32_t delta_us(timeval begin, timeval end);

void fset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value);
void rset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value);
void rget(shm_cache &cache, len_vector &key_len, char *key);
void rdel(shm_cache &cache, len_vector &key_len, char *key);

int main() {
    char *const key = (char *)malloc(MAX_KEY_SIZE * KEY_COUNT);
    memset(key, 0, MAX_KEY_SIZE * KEY_COUNT);
    char *const value = (char *)malloc(MAX_VALUE_SIZE);
    for (uint32_t i = 0; i < MAX_VALUE_SIZE; ++i) {
        *(value + i) = (char)('a' + i % 26);
    }
    len_vector key_len;
    len_vector value_len;
    for (uint32_t i = 0; i < KEY_COUNT; ++i) {
        string str = "key_" + to_string(i + 1);
        memcpy(key + i * MAX_KEY_SIZE, str.data(), str.length());
        key_len.push_back((uint32_t)str.length());
        value_len.push_back(rand_number(MIN_VALUE_SIZE, MAX_VALUE_SIZE));
    }

    const char *conf = "/tmp/cache.conf";
    shm_cache cache;
    if (cache.init(conf, true, true) != 0) {
        printf("cache init failed.\n");
        free(key);
        free(value);
        return 0;
    }

    fset(cache, key_len, value_len, key, value);
    rset(cache, key_len, value_len, key, value);
    cache.clear_global_stats();

    const int child = 6;
    pid_t process[child];
    for (int &pid : process) {
        pid = fork();
        if (pid == 0) {
            break;
        }
    }
    if (process[0] == 0) {
        cache.start_local_observe();
        rset(cache, key_len, value_len, key, value);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else if (process[1] == 0) {
        cache.start_local_observe();
        rget(cache, key_len, key);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else if (process[2] == 0) {
        cache.start_local_observe();
        rget(cache, key_len, key);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else if (process[3] == 0) {
        cache.start_local_observe();
        rget(cache, key_len, key);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else if (process[4] == 0) {
        cache.start_local_observe();
        rget(cache, key_len, key);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else if (process[5] == 0) {
        cache.start_local_observe();
        rget(cache, key_len, key);
        auto local_stats = cache.stop_local_observe(false, true);
        cout << local_stats << endl;
        exit(0);
    } else {
        for (int pid : process) {
            waitpid(pid, nullptr, 0);
        }
        auto global_stats = cache.get_global_stats().serialize();
        cout << "global status" << endl << global_stats << endl;
        cache.clear_global_stats();
        cache.remove();
        free(key);
        free(value);
        return 0;
    }
}

uint32_t rand_number(uint32_t min, uint32_t max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> uniform((int)min, (int)max);
    return (uint32_t)uniform(gen);
}

uint32_t delta_s(timeval begin, timeval end) {
    return uint32_t(end.tv_sec - begin.tv_sec);
}

uint32_t delta_ms(timeval begin, timeval end) {
    return uint32_t((uint64_t)(end.tv_usec - begin.tv_usec) / 1000 + (uint64_t)(end.tv_sec - begin.tv_sec) * 1000);
}

uint32_t delta_us(timeval begin, timeval end) {
    return uint32_t((uint64_t)(end.tv_usec - begin.tv_usec) + (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000);
}

void fset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value) {
    timeval begin;
    timeval end;
    int64_t time_sum = 0;
    for (uint32_t i = 0; i < KEY_COUNT; ++i) {
        key_info key_tmp(key_len[i], key + i * MAX_KEY_SIZE);
        value_info value_tmp(value_len[i], value, 1, 5400);
        gettimeofday(&begin, nullptr);
        int result = cache.set(key_tmp, value_tmp);
        gettimeofday(&end, nullptr);
        time_sum += delta_us(begin, end);
        if (result != 0) {
            printf("%d. set fail, errno: %d\n", i + 1, result);
        }
    }
    printf("pid: %d fset end, average = %f us.\n", getpid(), (float)time_sum / (float)(KEY_COUNT));
}

void rset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value) {
    timeval begin;
    timeval end;
    int64_t time_sum = 0;
    for (uint32_t count = 0; count < SET_TIMES; ++count) {
        auto number = rand_number(0u, KEY_COUNT - 1);
        key_info key_tmp(key_len[number % KEY_COUNT], key + (number % KEY_COUNT) * MAX_KEY_SIZE);
        value_info value_tmp(value_len[number % KEY_COUNT], value, 1, 5400);
        usleep(rand_number(MIN_SET_SLEEP_TIME, MAX_SET_SLEEP_TIME));
        gettimeofday(&begin, nullptr);
        int result = cache.set(key_tmp, value_tmp);
        gettimeofday(&end, nullptr);
        if (result != 0) {
            printf("%d. set fail, errno: %d\n", number + 1, result);
        }
        time_sum += delta_us(begin, end);
    }
    printf("pid: %d rand_set end, average = %f us.\n", getpid(), (float)time_sum / (float)(SET_TIMES));
}

void rget(shm_cache &cache, len_vector &key_len, char *key) {
    timeval begin;
    timeval end;
    auto *val_str = (char *)malloc(MAX_VALUE_SIZE);
    memset(val_str, 0, MAX_VALUE_SIZE);
    value_info val_tmp(MAX_VALUE_SIZE, val_str, 0, 0);
    int64_t time_sum = 0;
    for (uint32_t count = 0; count < GET_TIMES; ++count) {
        auto number = rand_number(0u, KEY_COUNT - 1);
        key_info key_tmp(key_len[number % KEY_COUNT], key + (number % KEY_COUNT) * MAX_KEY_SIZE);
        usleep(rand_number(MIN_GET_SLEEP_TIME, MAX_GET_SLEEP_TIME));
        gettimeofday(&begin, nullptr);
        cache.get(key_tmp, val_tmp, LRU_K);
        gettimeofday(&end, nullptr);
        time_sum += delta_us(begin, end);
    }
    printf("pid: %d rand_get end, average = %f us.\n", getpid(), (float)time_sum / (float)(GET_TIMES));
}

void rdel(shm_cache &cache, len_vector &key_len, char *key) {
    timeval begin;
    timeval end;
    set<uint32_t> deleted;
    int64_t time_sum = 0;
    for (uint32_t count = 0; count < DEL_TIMES; ++count) {
        auto number = rand_number(0u, KEY_COUNT - 1);
        if (deleted.find(number) == deleted.end()) {
            deleted.insert(number);
        } else {
            continue;
        }
        key_info key_tmp(key_len[number % KEY_COUNT], key + (number % KEY_COUNT) * MAX_KEY_SIZE);
        usleep(rand_number(MIN_DEL_SLEEP_TIME, MAX_DEL_SLEEP_TIME));
        gettimeofday(&begin, nullptr);
        cache.del(key_tmp);
        gettimeofday(&end, nullptr);
        time_sum += delta_us(begin, end);
    }
    printf("pid: %d rand_del end, average = %f us.\n", getpid(), (float)time_sum / (float)(DEL_TIMES));
}