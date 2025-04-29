#pragma once

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

#include "log.hpp"
#include "numtypes.hpp"

class BumpAllocator {
    std::vector<u8> storage_;
    size_t index_;
    bool out_of_memory_;

public:
    BumpAllocator(size_t size = 0) : storage_{}, index_{}, out_of_memory_{} { allocate(size); }

    template<typename T>
    T* acquire()
        requires(std::is_default_constructible_v<T> && std::is_trivially_destructible_v<T>)
    {
        assert(!storage_.empty());
        if constexpr (alignof(T) > 1) {
            size_t align_rem = index_ % alignof(T);
            if (align_rem) {
                index_ += alignof(T) - align_rem;
            }
        }
        if (index_ + sizeof(T) > storage_.size()) [[unlikely]] {
            if (!std::exchange(out_of_memory_, true)) {
                LogWarn("Bump allocator ran out of memory ({} bytes)", storage_.size());
            }
            return nullptr;
        }
        T* obj = ::new (static_cast<void*>(&storage_[index_])) T{};
        index_ += sizeof(T);
        return obj;
    }

    void allocate(size_t size)
    {
        storage_.resize(size, 0);
        index_ = 0;
        out_of_memory_ = false;
    }

    void deallocate()
    {
        storage_ = {};
        index_ = 0;
        out_of_memory_ = false;
    }

    bool out_of_memory() const { return out_of_memory_; }

    void reset()
    {
        std::ranges::fill(storage_, 0);
        index_ = 0;
        out_of_memory_ = false;
    }
};
