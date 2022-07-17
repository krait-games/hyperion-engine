#ifndef HYPERION_V2_LIB_CONTAINER_BASE_H
#define HYPERION_V2_LIB_CONTAINER_BASE_H

#include <util/Defines.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Container, class Key>
class ContainerBase {
protected:
    using Base     = ContainerBase;
    using KeyType  = Key;

public:
    ContainerBase() {}
    ~ContainerBase() {}

    template <class T>
    [[nodiscard]] auto Find(const T &value)
    {
        auto _begin     = static_cast<Container *>(this)->Begin();
        const auto _end = static_cast<Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (*_begin == value) {
                return _begin;
            }
        }

        return _begin;
    }
    
    template <class T>
    [[nodiscard]] auto Find(const T &value) const
    {
        auto _begin     = static_cast<const Container *>(this)->Begin();
        const auto _end = static_cast<const Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (*_begin == value) {
                return _begin;
            }
        }

        return _begin;
    }

    template <class Function>
    [[nodiscard]] auto FindIf(Function &&pred)
    {
        typename Container::Iterator _begin     = static_cast<Container *>(this)->Begin();
        const typename Container::Iterator _end = static_cast<Container *>(this)->End();

        for (; _begin != _end; ++_begin) {
            if (pred(*_begin)) {
                return _begin;
            }
        }

        return _begin;
    }
    
    template <class T>
    [[nodiscard]] auto LowerBound(const T &key)
    {
        const auto _begin = static_cast<Container *>(this)->Begin();
        const auto _end   = static_cast<Container *>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key
        );
    }
    
    template <class T>
    [[nodiscard]] auto LowerBound(const T &key) const
    {
        const auto _begin = static_cast<const Container *>(this)->Begin();
        const auto _end   = static_cast<const Container *>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key
        );
    }

    template <class T>
    [[nodiscard]] bool Contains(const T &value) const
    {
        return static_cast<const Container *>(this)->Find(value)
            != static_cast<const Container *>(this)->End();
    }
};


} // namespace hyperion

#endif
