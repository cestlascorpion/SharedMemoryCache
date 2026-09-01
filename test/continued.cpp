#include "../src/shm_cache.h"
#include <cassert>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    shm_cache cache;
    assert(cache.init("cfg/continued.conf", true, true) == 0);

    char key[] = "key";
    char val[] = "value";
    assert(cache.set_ex(key, val, 1, 0) == 0);

    char small[3] = {};
    assert(cache.get_ex(key, small, sizeof(small), 0) == ENOSPC);

    char out[16] = {};
    assert(cache.get_ex(key, out, sizeof(out), 0) == 0);
    assert(std::memcmp(out, val, sizeof(val) - 1) == 0);
    sleep(2);
    assert(cache.get_ex(key, out, sizeof(out), 0) == ETIMEDOUT);

    char key2[] = "key2";
    assert(cache.set_ex(key2, val, 0, 0) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char child_val[] = "child";
        _exit(cache.set_ex(key2, child_val, 0, 0) == 0 ? 0 : 1);
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(cache.get_ex(key2, out, sizeof(out), 0) == 0);
    assert(std::memcmp(out, "child", 5) == 0);

    assert(cache.destroy() == 0);
    assert(cache.remove() == 0);
    return 0;
}
