#ifndef SHMCACHE_SHM_CONFIGURE_H
#define SHMCACHE_SHM_CONFIGURE_H

#include <string>
#include <unordered_map>

class shm_configure {
public:
    explicit shm_configure(const char *file);
    int64_t get_integer_value(const std::string &key);
    std::string get_string_value(const std::string &key);

private:
    void split_and_store(const std::string &str);

private:
    std::unordered_map<std::string, std::string> m_conf;
};

#endif // SHMCACHE_SHM_CONFIGURE_H
