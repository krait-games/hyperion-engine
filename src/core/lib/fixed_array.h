#ifndef HYPERION_V2_LIB_FIXED_ARRAY_H
#define HYPERION_V2_LIB_FIXED_ARRAY_H

#include <util/defines.h>
#include <types.h>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T, UInt Sz>
class FixedArray {
    T m_data[Sz];

public:
    using Iterator      = T *;
    using ConstIterator = const T *;

    FixedArray();

    FixedArray(const T items[Sz])
    {
        for (UInt i = 0; i < Sz; i++) {
            m_data[i] = items[i];
        }
    }

    FixedArray &operator=(const T items[Sz])
    {
        for (UInt i = 0; i < Sz; i++) {
            m_data[i] = items[i];
        }

        return *this;
    }

    FixedArray(const std::initializer_list<T> &items)
    {
        if (items.size() <= Sz) {
            std::copy(
                items.begin(),
                items.end(),
                std::begin(m_data)
            );
        } else {
            std::copy(
                items.begin(),
                items.begin() + Sz,
                std::begin(m_data)
            );
        }
    }

    FixedArray(const FixedArray &other);
    FixedArray &operator=(const FixedArray &other);
    FixedArray(FixedArray &&other) noexcept;
    FixedArray &operator=(FixedArray &&other) noexcept;
    ~FixedArray();

    HYP_FORCE_INLINE
    T &operator[](UInt index)               { return m_data[index]; }

    HYP_FORCE_INLINE
    const T &operator[](UInt index) const   { return m_data[index]; }

    HYP_FORCE_INLINE
    UInt Size() const                       { return Sz; }

    HYP_FORCE_INLINE
    bool Empty() const                      { return Sz == 0; }

    HYP_FORCE_INLINE
    bool Any() const                        { return Sz != 0; }

    HYP_FORCE_INLINE
    T *Data()                               { return static_cast<T *>(m_data); }

    HYP_FORCE_INLINE
    const T *Data() const                   { return static_cast<const T *>(m_data); }

    HYP_DEF_STL_BEGIN_END(&m_data[0], &m_data[Sz])
};

template <class T, UInt Sz>
FixedArray<T, Sz>::FixedArray()
    : m_data{}
{
}

template <class T, UInt Sz>
FixedArray<T, Sz>::FixedArray(const FixedArray &other)
{
    for (UInt i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }
}

template <class T, UInt Sz>
auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
{
    for (UInt i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }

    return *this;
}

template <class T, UInt Sz>
FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
{
    for (UInt i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
    }
}

template <class T, UInt Sz>
auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
{
    for (UInt i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
    }

    return *this;
}

template <class T, UInt Sz>
FixedArray<T, Sz>::~FixedArray() = default;

} // namespace hyperion

#endif
