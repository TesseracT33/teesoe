#pragma once

#include "numtypes.hpp"
#include <array>
#include <cstring>
#include <utility>

using u8 = unsigned char;

template<typename T> class DoNotDestroy {
    static constexpr size_t size = sizeof(T);

public:
    DoNotDestroy(T&& t) { memcpy(storage.data(), &t, size); }

    template<typename... U>
    DoNotDestroy(std::in_place_t, U... u)
        requires(std::constructible_from<T, U...>)
    {
        T t{ u... };
        memcpy(storage.data(), &t, size);
    }

    T* operator->() { return reinterpret_cast<T*>(storage.data()); }
    T const* operator->() const { return reinterpret_cast<T const*>(storage.data()); }
    T* operator*() { return reinterpret_cast<T*>(storage.data()); }
    T const* operator*() const { return reinterpret_cast<T const*>(storage.data()); }

private:
    std::array<u8, size> storage;
};
