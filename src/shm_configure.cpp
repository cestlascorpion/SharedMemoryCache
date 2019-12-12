#include "shm_configure.h"
#include <fstream>
#include <unistd.h>

shm_configure::shm_configure(const char *file) {
    std::fstream conf;
    conf.open(file, std::ios::in);
    if (!conf.is_open()) {
        printf("%s %s: pid: %d open() failed.\n", __FILE__, __func__, getpid());
        return;
    }
    std::string line;
    while (getline(conf, line)) {
        if (line.empty()) {
            break;
        }
        split_and_store(line);
    }
    conf.close();
}

int64_t shm_configure::get_integer_value(const std::string &key) {
    if (m_conf.find(key) != m_conf.end()) {
        std::string value = m_conf[key];
        char end = value[value.length() - 1];
        if (end >= '0' && end <= '9') {
            return stoi(value);
        } else {
            int64_t factor = 1;
            switch (end) {
            case 'K':
                factor = 1024;
                break;
            case 'M':
                factor = 1024 * 1024;
                break;
            case 'G':
                factor = 1024 * 1024 * 1024;
                break;
            default:
                printf("%s %s: pid: %d configure file has a wrong unit.\n", __FILE__, __func__, getpid());
            }
            value.pop_back();
            return (int64_t)stoi(value) * factor;
        }
    } else {
        return -1;
    }
}

std::string shm_configure::get_string_value(const std::string &key) {
    if (m_conf.find(key) != m_conf.end()) {
        return m_conf[key];
    } else {
        printf("%s %s: pid: %d configure file may be wrong.\n", __FILE__, __func__, getpid());
        return "null";
    }
}

void shm_configure::split_and_store(const std::string &str) {
    unsigned idx = 0;
    while (idx < str.length() && str[idx] == ' ')
        ++idx;
    if (idx == str.length())
        return;
    if (str[idx] == '#')
        return;
    unsigned edx = idx + 1;
    while (edx < str.length() && str[edx] != ' ')
        ++edx;
    if (edx == str.length())
        return;
    std::string key = str.substr(idx, edx - idx);
    idx = edx + 3;
    if (idx >= str.length() || str.substr(edx, idx - edx) != " = ")
        return;
    edx = idx + 1;
    while (edx < str.length() && str[edx] != ' ' && str[edx] != '\r' && str[edx] != '\n')
        ++edx;
    std::string value = str.substr(idx, edx - idx);
    if (edx != str.length()) {
        while (edx != str.length()) {
            if (str[edx] != ' ')
                return;
            ++edx;
        }
    }
    m_conf[key] = value;
}
