#include "../src/shm_cache.h"
#include <cassert>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *conf = argc > 1 ? argv[1] : "cfg/attach.conf";
    shm_cache cache;
    assert(cache.init(conf, true, true) == 0);

    char key_a[] = "a";
    char key_b[] = "b";
    char key_c[] = "c";
    char value[] = "value";
    assert(cache.set_ex(key_a, value, 0, 0) == 0);
    assert(cache.set_ex(key_b, value, 0, 0) == 0);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        shm_cache attached;
        if (attached.init(conf, false, true) != 0) {
            _exit(1);
        }
        char output[16] = {};
        if (attached.get_ex(key_a, output, sizeof(output), 0) != 0 ||
            std::memcmp(output, value, sizeof(value) - 1) != 0) {
            _exit(2);
        }
        _exit(0);
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    char output[16] = {};
    assert(cache.set_ex(key_c, value, 0, 0) == 0);
    assert(cache.get_ex(key_c, output, sizeof(output), 0) == 0);
    assert(cache.get_ex(key_b, output, sizeof(output), 0) != 0);

    assert(cache.destroy() == 0);
    assert(cache.remove() == 0);
    return 0;
}
