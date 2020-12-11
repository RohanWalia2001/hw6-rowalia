#include <string>

size_t hash_33(const std::string &key) {
    size_t hash = 5381;
    for (char i : key) hash = hash * 33 + static_cast<unsigned char>(i);
    return hash;
}
