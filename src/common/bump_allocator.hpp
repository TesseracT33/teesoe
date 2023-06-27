#pragma once

#include "log.hpp"
#include "types.hpp"

#include <algorithm>
#include <vector>

class BumpAllocator {
public:
    BumpAllocator(size_t size = 0) : memory_{}, index_{}, ran_out_of_memory_on_last_acquire_{} { allocate(size); }

    u8* acquire(size_t size)
    {
        if (index_ + size >= memory_.size()) {
            log_warn("Bump allocator ran out of memory; resetting all available memory.");
            std::ranges::fill(memory_, 0);
            index_ = 0;
            ran_out_of_memory_on_last_acquire_ = true;
        } else {
            ran_out_of_memory_on_last_acquire_ = false;
        }
        u8* alloc = &memory_[index_];
        index_ += size;
        return alloc;
    }

    void allocate(size_t size)
    {
        memory_.resize(size, 0);
        index_ = 0;
    }

    void deallocate() { memory_.clear(); }

    bool ran_out_of_memory_on_last_acquire() const { return ran_out_of_memory_on_last_acquire_; }

private:
    std::vector<u8> memory_;
    size_t index_;
    bool ran_out_of_memory_on_last_acquire_;
};
