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

using namespace std;

const uint32_t MAX_KEY_SIZE = 64;
const uint32_t MAX_VALUE_SIZE = 4 * 1024 * 1024;
const uint32_t MIN_VALUE_SIZE = 2 * 1024 * 1024;
const uint32_t MAX_SET_SLEEP_TIME = 5000;
const uint32_t MIN_SET_SLEEP_TIME = 5000;
const uint32_t MAX_GET_SLEEP_TIME = 5000;
const uint32_t MIN_GET_SLEEP_TIME = 5000;
const uint32_t MAX_DEL_SLEEP_TIME = 5000;
const uint32_t MIN_DEL_SLEEP_TIME = 5000;
const uint32_t KEY_COUNT = 2000;
const uint32_t SET_TIMES = 1000;
const uint32_t GET_TIMES = 1000;
const uint32_t DEL_TIMES = 100;
const uint32_t LRU_K = 1;

using len_vector = vector<uint32_t>;
uint32_t rand_number(uint32_t min, uint32_t max);
uint32_t delta_ms(timeval begin, timeval end);
uint32_t delta_us(timeval begin, timeval end);

void fset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value);
void rset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value);
void rget(shm_cache &cache, len_vector &key_len, char *key);
void rdel(shm_cache &cache, len_vector &key_len, char *key);

int main() {
    const char *conf = "/tmp/cache.conf";
    char *key = (char *)malloc(MAX_KEY_SIZE * KEY_COUNT);
    memset(key, 0, MAX_KEY_SIZE * KEY_COUNT);
    char *value = (char *)malloc(MAX_VALUE_SIZE);
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

    shm_cache cache;
    if (cache.init(conf, true, true) != 0) {
        printf("cache init failed.\n");
        free(key);
        free(value);
        return 0;
    }
    cache.clear_global_stats();
    cache.start_local_observe();
    fset(cache, key_len, value_len, key, value);
    rdel(cache, key_len, key);
    rget(cache, key_len, key);
    rset(cache, key_len, value_len, key, value);
    rdel(cache, key_len, key);
    rget(cache, key_len, key);

    auto local_stats = cache.stop_local_observe(false, true);
    cout << "local_stats" << endl << local_stats << endl;
    auto global_stats = cache.get_global_stats().serialize();
    cout << "global status" << endl << global_stats << endl;
    cache.clear_global_stats();

    free(key);
    free(value);

    // cache.destroy();
    cache.remove();
    return 0;
}

uint32_t rand_number(uint32_t min, uint32_t max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> uniform((int)min, (int)max);
    return (uint32_t)uniform(gen);
}

uint32_t delta_ms(timeval begin, timeval end) {
    return uint32_t((uint64_t)(end.tv_usec - begin.tv_usec) / 1000 + (uint64_t)(end.tv_sec - begin.tv_sec) * 1000);
}

uint32_t delta_us(timeval begin, timeval end) {
    return uint32_t((uint64_t)(end.tv_usec - begin.tv_usec) + (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000);
}

void fset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value) {
    timeval now;
    timeval begin;
    timeval end;
    gettimeofday(&now, nullptr);
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
    gettimeofday(&now, nullptr);
    printf("pid: %d fset end, average = %f us.\n", getpid(), (float)time_sum / (float)(KEY_COUNT));
}

void rset(shm_cache &cache, len_vector &key_len, len_vector &value_len, char *key, char *value) {
    timeval now;
    timeval begin;
    timeval end;
    gettimeofday(&now, nullptr);
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
    gettimeofday(&now, nullptr);
    printf("pid: %d rand_set end, average = %f us.\n", getpid(), (float)time_sum / (float)(SET_TIMES));
}

void rget(shm_cache &cache, len_vector &key_len, char *key) {
    timeval now;
    timeval begin;
    timeval end;
    gettimeofday(&now, nullptr);
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
    gettimeofday(&now, nullptr);
    printf("pid: %d rand_get end, average = %f us.\n", getpid(), (float)time_sum / (float)(GET_TIMES));
}

void rdel(shm_cache &cache, len_vector &key_len, char *key) {
    timeval now;
    timeval begin;
    timeval end;
    gettimeofday(&now, nullptr);
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
    gettimeofday(&now, nullptr);
    printf("pid: %d rand_del end, average = %f us.\n", getpid(), (float)time_sum / (float)(DEL_TIMES));
}