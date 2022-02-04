#ifndef HASH_CODE_H
#define HASH_CODE_H

#include <type_traits>
#include <functional>

namespace hyperion {
using HashCode_t = size_t;

struct HashCode {
    using Value_t = HashCode_t;

    HashCode()
        : hash(0)
    {
    }

    HashCode(const HashCode &other)
        : hash(other.hash)
    {
    }

    template<class T>
    typename std::enable_if<std::is_same<T, HashCode>::value || std::is_base_of<HashCode, T>::value>::type
    Add(const T &hash_code)
    {
        HashCombine(hash_code.Value());
    }

    template<class T>
    typename std::enable_if<std::is_fundamental<T>::value || std::is_same<T, std::string>::value>::type
    Add(const T &value)
    {
        HashCombine(std::hash<T>()(value));
    }

    Value_t Value() const
    {
        return hash;
    }

private:
    Value_t hash;

    void HashCombine(Value_t other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

#endif
