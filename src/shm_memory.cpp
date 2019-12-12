#include "shm_memory.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

void *shm_memory::map(uint32_t type, const char *file, uint32_t id, uint32_t size, key_t &key, bool create,
                      int &error) {
    error = get_key(file, id, key);
    if (error != 0) {
        printf("%s %s: pid: %d get_key() failed.\n", __FILE__, __func__, getpid());
        return nullptr;
    }
    if (type == SHM_MEM_TYPE_MMAP) {
        return do_mmap(file, id, size, create, error);
    } else {
        return do_shmmap(key, size, create, error);
    }
}

int shm_memory::unmap(uint32_t type, void *addr, uint32_t size) {
    int res;
    if (type == SHM_MEM_TYPE_MMAP) {
        if (munmap(addr, size) == 0) {
            res = 0;
        } else {
            res = (errno != 0 ? errno : EACCES);
            printf("%s %s: pid: %d munmap() failed.\n", __FILE__, __func__, getpid());
        }
    } else {
        if (shmdt(addr) == 0) {
            res = 0;
        } else {
            res = (errno != 0 ? errno : EACCES);
            printf("%s %s: pid: %d shmdt() failed.\n", __FILE__, __func__, getpid());
        }
    }
    return res;
}

int shm_memory::remove(uint32_t type, const char *file, uint32_t id, key_t key) {
    int res;
    if (type == SHM_MEM_TYPE_MMAP) {
        char true_file[SHM_MAX_PATH_SIZE];
        get_true_file(true_file, file, id);

        if (unlink(true_file) != 0) {
            res = (errno != 0 ? errno : EPERM);
            printf("%s %s: pid: %d unlink() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
    } else {
        int shm_id = shmget(key, 0, 0666);
        if (shm_id < 0) {
            res = (errno != 0 ? errno : EACCES);
            printf("%s %s: pid: %d shmget() failed.\n", __FILE__, __func__, getpid());
            return res;
        }

        if (shmctl(shm_id, IPC_RMID, nullptr) != 0) {
            res = (errno != 0 ? errno : EACCES);
            printf("%s %s: pid: %d shmctl() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
    }
    return 0;
}

bool shm_memory::exists(uint32_t type, const char *file, uint32_t id) {
    key_t key;
    if (get_key(file, id, key) != 0) {
        return false;
    }
    if (type == SHM_MEM_TYPE_MMAP) {
        char true_file[SHM_MAX_PATH_SIZE];
        get_true_file(true_file, file, id);
        return access(true_file, F_OK) == 0;
    } else {
        return shmget(key, 0, 0666) >= 0;
    }
}

void *shm_memory::do_mmap(const char *file, uint32_t id, uint32_t size, bool create, int &error) {
    char true_file[SHM_MAX_PATH_SIZE];
    get_true_file(true_file, file, id);
    int fd = open(true_file, O_RDWR);
    void *addr;
    mode_t old_mast;
    bool need_truncate;
    if (fd >= 0) {
        struct stat st;
        if (fstat(fd, &st) != 0) {
            error = (errno != 0 ? errno : EPERM);
            printf("%s %s: pid: %d fstat() failed.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
        if (st.st_size < (off_t)size) {
            printf("%s %s: pid: %d fst.st_size < size.\n", __FILE__, __func__, getpid());
            need_truncate = true;
        } else {
            if (st.st_size > (off_t)size) {
                error = (errno != 0 ? errno : EPERM);
                printf("%s %s: pid: %d fst.st_size > size.\n", __FILE__, __func__, getpid());
            }
            need_truncate = false;
        }
    } else {
        if (!(create && errno == ENOENT)) {
            error = (errno != 0 ? errno : EPERM);
            printf("%s %s: pid: %d open() failed.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
        old_mast = umask(0);
        fd = open(true_file, O_RDWR | O_CREAT, 0666);
        umask(old_mast);

        if (fd < 0) {
            error = (errno != 0 ? errno : EPERM);
            printf("%s %s: pid: %d open() failed x2.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
        need_truncate = true;
    }
    if (need_truncate) {
        if (ftruncate(fd, (off_t)size) != 0) {
            error = (errno != 0 ? errno : EPERM);
            close(fd);
            printf("%s %s: pid: %d ftruncate() failed.\n", __FILE__, __func__, getpid());
            return nullptr;
        }
    }
    addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == nullptr) {
        error = (errno != 0 ? errno : EPERM);
        printf("%s %s: pid: %d mmap() failed.\n", __FILE__, __func__, getpid());
    }
    close(fd);
    error = 0;
    return addr;
}

void *shm_memory::do_shmmap(key_t &key, uint32_t size, bool create, int &error) {
    int shm_id;
    if (create) {
        shm_id = shmget(key, size, IPC_CREAT | 0666);
    } else {
        shm_id = shmget(key, 0, 0666);
    }
    if (shm_id < 0) {
        error = (errno != 0 ? errno : EPERM);
        printf("%s %s: pid: %d shmget() failed.\n", __FILE__, __func__, getpid());
        return nullptr;
    }
    void *addr = shmat(shm_id, nullptr, 0);
    if (addr == nullptr || addr == (void *)-1) {
        error = (errno != 0 ? errno : EPERM);
        printf("%s %s: pid: %d shmat() failed.\n", __FILE__, __func__, getpid());
        return nullptr;
    }
    error = 0;
    return addr;
}

void shm_memory::get_true_file(char *true_file, const char *file, uint32_t id) {
    memset(true_file, 0, SHM_MAX_PATH_SIZE);
    snprintf(true_file, SHM_MAX_PATH_SIZE, "%s.%d", file, id - 1);
}

int shm_memory::get_key(const char *file, uint32_t id, key_t &key) {
    if (access(file, F_OK) != 0) {
        int res = (errno != 0 ? errno : ENOENT);
        if (res != ENOENT) {
            printf("%s %s: pid: %d access() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
        res = write_file(file, "FOR LOCK", 8);
        if (res != 0) {
            printf("%s %s: pid: %d write_file() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
        if (chmod(file, 0666) != 0) {
            res = (errno != 0 ? errno : EFAULT);
            printf("%s %s: pid: %d chmod() failed.\n", __FILE__, __func__, getpid());
            return res;
        }
    }
    key = ftok(file, (int)id);
    if (key == -1) {
        int res = (errno != 0 ? errno : EFAULT);
        printf("%s %s: pid: %d ftok() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    return 0;
}

int shm_memory::write_file(const char *file, const char *buff, uint32_t size) {
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        int res = (errno != 0 ? errno : EIO);
        printf("%s %s: pid: %d open() failed.\n", __FILE__, __func__, getpid());
        return res;
    }
    if (write(fd, buff, (size_t)size) != size) {
        int res = (errno != 0 ? errno : EIO);
        printf("%s %s: pid: %d write() failed.\n", __FILE__, __func__, getpid());
        close(fd);
        return res;
    }
    if (fsync(fd) != 0) {
        int res = (errno != 0 ? errno : EIO);
        printf("%s %s: pid: %d fsync() failed.\n", __FILE__, __func__, getpid());
        close(fd);
        return res;
    }
    close(fd);
    return 0;
}