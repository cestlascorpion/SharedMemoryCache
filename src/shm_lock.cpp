#include "shm_lock.h"
#include "shm_hashtable.h"
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

int shm_lock::lock_init(context &context) {
    int res;
    pthread_mutexattr_t mat;
    if ((res = pthread_mutexattr_init(&mat)) != 0) {
        printf("%s %s: pid: %d pthread_mutexattr_init() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    if ((res = pthread_mutexattr_setpshared(&mat, PTHREAD_PROCESS_SHARED)) != 0) {
        printf("%s %s: pid: %d pthread_mutexattr_setpshared() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    if ((res = pthread_mutexattr_settype(&mat, PTHREAD_MUTEX_NORMAL)) != 0) {
        printf("%s %s: pid: %d pthread_mutexattr_settype() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
#if defined(PTHREAD_MUTEX_ROBUST)
    if ((res = pthread_mutexattr_setrobust(&mat, PTHREAD_MUTEX_ROBUST)) != 0) {
        printf("%s %s: pid: %d pthread_mutexattr_setrobust() failed.\n", __FILE__, __func__, getpid());
        pthread_mutexattr_destroy(&mat);
        return res;
    }
#endif
    if ((res = pthread_mutex_init(&context.memory->global_lock.mutex, &mat)) != 0) {
        printf("%s %s: pid: %d pthread_mutex_init() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    pthread_mutexattr_destroy(&mat);
    context.memory->global_lock.owner = -1;
    return 0;
}

int shm_lock::read_lock(context &context, const config &config, global_stats &global_stats) {
    int res;
    __sync_add_and_fetch(&global_stats.r_lock_total, 1);
    uint32_t ticks = 0;
    while ((res = pthread_mutex_trylock(&context.memory->global_lock.mutex)) == EBUSY) {
        __sync_add_and_fetch(&global_stats.r_lock_retry, 1);
        usleep(config.try_r_lk_interval);
        ++ticks;
        if (ticks > config.detect_r_dl_ticks) {
            ticks = 0;
            pid_t pid = context.memory->global_lock.owner;
            if (pid > 0 && (kill(pid, 0) != 0) && (errno == ESRCH || errno == ENOENT)) {
                __sync_add_and_fetch(&global_stats.detect_deadlock, 1);
                int recovery = handle_deadlock(context, config, global_stats);
                if (recovery != 0) {
                    return recovery;
                }
            }
        }
    }
#if defined(PTHREAD_MUTEX_ROBUST)
    if (res == EOWNERDEAD) {
        shm_hashtable::ht_clear(context, global_stats);
        res = pthread_mutex_consistent(&context.memory->global_lock.mutex);
    }
#endif
    if (res != 0) {
        printf("%s %s: pid: %d error %d.\n", __FILE__, __func__, getpid(), res);
    } else {
        context.memory->global_lock.owner = getpid();
    }
    return res;
}

int shm_lock::read_unlock(context &context) {
    int res;
    context.memory->global_lock.owner = -1;
    res = pthread_mutex_unlock(&context.memory->global_lock.mutex);
    if (res != 0) {
        printf("%s %s: pid: %d read_unlock() failed.\n", __FILE__, __func__, getpid());
    }
    return res;
}

int shm_lock::write_lock(context &context, const config &config, global_stats &global_stats) {
    int res;
    __sync_add_and_fetch(&global_stats.w_lock_total, 1);
    uint32_t ticks = 0;
    while ((res = pthread_mutex_trylock(&context.memory->global_lock.mutex)) == EBUSY) {
        __sync_add_and_fetch(&global_stats.w_lock_retry, 1);
        usleep(config.try_w_lk_interval);
        ++ticks;
        if (ticks > config.detect_w_dl_ticks) {
            ticks = 0;
            pid_t pid = context.memory->global_lock.owner;
            if (pid > 0 && (kill(pid, 0) != 0) && (errno == ESRCH || errno == ENOENT)) {
                __sync_add_and_fetch(&global_stats.detect_deadlock, 1);
                int recovery = handle_deadlock(context, config, global_stats);
                if (recovery != 0) {
                    return recovery;
                }
            }
        }
    }
#if defined(PTHREAD_MUTEX_ROBUST)
    if (res == EOWNERDEAD) {
        shm_hashtable::ht_clear(context, global_stats);
        res = pthread_mutex_consistent(&context.memory->global_lock.mutex);
    }
#endif
    if (res != 0) {
        printf("%s %s: pid: %d error %d.\n", __FILE__, __func__, getpid(), res);
    } else {
        context.memory->global_lock.owner = getpid();
    }
    return res;
}

int shm_lock::write_unlock(context &context) {
    int res;
    context.memory->global_lock.owner = -1;
    res = pthread_mutex_unlock(&context.memory->global_lock.mutex);
    if (res != 0) {
        printf("%s %s: pid: %d w_lock() failed.\n", __FILE__, __func__, getpid());
    }
    return res;
}

int shm_lock::file_lock(context &context, const config &config) {
    int res;
    if (context.lock_fd >= 0) {
        close(context.lock_fd);
    }
    mode_t old_mast = umask(0);
    context.lock_fd = open(config.file, O_WRONLY | O_CREAT, 0666);
    umask(old_mast);
    if (context.lock_fd < 0) {
        res = (errno != 0 ? errno : EPERM);
        printf("%s %s: pid: %d open() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    res = file_write_lock(context.lock_fd);
    if (res != 0) {
        close(context.lock_fd);
        context.lock_fd = -1;
        printf("%s %s: pid: %d file_write_lock() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    return res;
}

void shm_lock::file_unlock(context &context) {
    close(context.lock_fd);
    context.lock_fd = -1;
}

int shm_lock::handle_deadlock(context &context, const config &config, global_stats &global_stats) {
    (void)context;
    (void)config;
    (void)global_stats;
    printf("%s %s: pid: %d cannot recover a non-robust mutex safely.\n", __FILE__, __func__, getpid());
    return EDEADLK;
}

int shm_lock::file_write_lock(const int fd) {
    int res;
    struct flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    do {
        if ((res = fcntl(fd, F_SETLKW, &lock)) != 0) {
            res = (errno != 0 ? errno : ENOMEM);
            printf("%s %s: pid: %d fcntl() failed.\n", __FILE__, __func__, getpid());
        }
    } while (res == EINTR);
    return res;
}
