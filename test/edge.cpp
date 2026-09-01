#include "../src/shm_cache.h"
#include <cassert>
#include <cstring>
#include <vector>

int main() {
    shm_cache cache;
    assert(cache.init("cfg/edge.conf", true, true) == 0);

    char key_data[] = "large";
    key_info key((uint32_t)5, key_data);
    std::vector<char> first(100000);
    for (size_t i = 0; i < first.size(); ++i) {
        first[i] = (char)('a' + i % 26);
    }
    value_info first_value((uint32_t)first.size(), first.data(), 7, 0);
    assert(cache.set(key, first_value) == 0);

    std::vector<char> output(first.size() + 1);
    value_info result((uint32_t)output.size(), output.data(), 0, 0);
    assert(cache.get(key, result, 0) == 0);
    assert(result.length == first.size());
    assert(std::memcmp(output.data(), first.data(), first.size()) == 0);
    assert(result.options == 7);

    std::vector<char> second(180000, 'z');
    value_info second_value((uint32_t)second.size(), second.data(), 9, 0);
    assert(cache.set(key, second_value) == 0);
    output.resize(second.size() + 1);
    result.data = output.data();
    result.length = (uint32_t)output.size();
    assert(cache.get(key, result, 0) == 0);
    assert(result.length == second.size());
    assert(std::memcmp(output.data(), second.data(), second.size()) == 0);
    assert(result.options == 9);

    assert(cache.set_ttl(key, 1) == 0);
    assert(cache.get(key, result, 0) == 0);
    assert(cache.get_ex(key_data, output.data(), 0, 0) == EINVAL);
    assert(cache.set_ex(nullptr, key_data, 0, 0) == EINVAL);

    assert(cache.clear_hashtable() >= 1);
    assert(cache.get(key, result, 0) == ENOENT);
    assert(cache.remove() == 0);
    assert(cache.destroy() == 0);
    return 0;
}
