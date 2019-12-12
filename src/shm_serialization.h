#ifndef SHMCACHE_SHM_SERIALIZATION_H
#define SHMCACHE_SHM_SERIALIZATION_H

#include <string>
#include <vector>

class shm_serialization {
public:
    void put_data(std::string &lvalue, std::string &rvalue);
    void write_data(std::string &dir);
    std::string simple_serialize();

private:
    std::vector<std::pair<std::string, std::string>> m_content;
};

#endif // SHMCACHE_SHM_SERIALIZATION_H
