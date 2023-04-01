#pragma once

#include "log.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <vector>

class BumpAllocator {
public:
    BumpAllocator(size_t size) : memory(size, 0), index(0) {}

    u8* acquire(size_t size)
    {
        if (index >= size) {
            log_warn("[JIT] ran out of space for pool allocator; resetting all available memory.");
            std::ranges::fill(memory, 0);
            index = 0;
        }
        u8* ret = &memory[index];
        index += size;
        return ret;
    }

private:
    std::vector<u8> memory;
    size_t index;
};
