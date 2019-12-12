#include "shm_serialization.h"
#include <fstream>
#include <unistd.h>

void shm_serialization::put_data(std::string &lvalue, std::string &rvalue) {
    if (lvalue.empty() || rvalue.empty()) {
        return;
    }
    m_content.emplace_back(move(lvalue), move(rvalue));
}

void shm_serialization::write_data(std::string &dir) {
    std::string file = dir + "cache." + std::to_string(getpid()) + "." + std::to_string(time(nullptr)) + ".txt";
    if (!m_content.empty()) {
        std::fstream log;
        log.open(file, std::ios::out | std::ios::ate);
        if (!log.is_open()) {
            printf("%s %s: pid: %d open(%s) failed.\n", __FILE__, __func__, getpid(), file.c_str());
            return;
        }
        for (auto &pair : m_content) {
            log << pair.first + ":" + pair.second << std::endl;
        }
        log.close();
    }
}

std::string shm_serialization::simple_serialize() {
    if (m_content.empty()) {
        return std::string();
    }
    std::string res;
    for (auto &pair : m_content) {
        res += "[" + pair.first + ":" + pair.second + "]\n";
    }
    m_content.clear();
    return res;
}
