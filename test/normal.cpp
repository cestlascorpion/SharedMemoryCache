#include "../src/shm_cache.h"
#include <cassert>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    shm_cache cache;
    assert(cache.init("cfg/normal.conf", true, true) == 0);
    cache.start_local_observe();

    char key[] = "normal";
    char value[] = "value";
    char output[32] = {};
    assert(cache.set_ex(key, value, 0, 3) == 0);
    assert(cache.set_ttl_ex(key, 5) == 0);
    assert(cache.get_ex(key, output, sizeof(output), 0) == 0);
    assert(std::memcmp(output, value, sizeof(value) - 1) == 0);
    assert(cache.set_expires_ex(key, (int)time(nullptr) + 5) == 0);
    assert(cache.del_ex(key) == 0);
    assert(cache.get_ex(key, output, sizeof(output), 0) != 0);

    const int workers = 4;
    pid_t pids[workers];
    for (int i = 0; i < workers; ++i) {
        pids[i] = fork();
        assert(pids[i] >= 0);
        if (pids[i] == 0) {
            char worker_key[32];
            char worker_value[32];
            char worker_output[32] = {};
            snprintf(worker_key, sizeof(worker_key), "worker_%d", i);
            snprintf(worker_value, sizeof(worker_value), "value_%d", i);
            for (int n = 0; n < 100; ++n) {
                if (cache.set_ex(worker_key, worker_value, 0, 0) != 0 ||
                    cache.get_ex(worker_key, worker_output, sizeof(worker_output), 0) != 0 ||
                    std::memcmp(worker_output, worker_value, strlen(worker_value)) != 0) {
                    _exit(1);
                }
            }
            _exit(0);
        }
    }
    for (pid_t pid : pids) {
        int status = 0;
        assert(waitpid(pid, &status, 0) == pid);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    std::string local = cache.stop_local_observe(false, true);
    assert(!local.empty());
    auto stats = cache.get_global_stats();
    assert(stats.global_stats.set.total >= (uint32_t)(workers * 100));
    assert(stats.global_stats.get.total >= (uint32_t)(workers * 100));
    assert(cache.clear_global_stats() == 0);
    assert(cache.remove() == 0);
    assert(cache.destroy() == 0);
    return 0;
}
