#pragma once

#include "log.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <vector>

class BumpAllocator {
public:
    BumpAllocator(size_t size = 0) : memory(size, 0), index(0) {}

    u8* acquire(size_t size)
    {
        if (index >= memory.size()) {
            log_warn("[JIT] ran out of space for pool allocator; resetting all available memory.");
            std::ranges::fill(memory, 0);
            index = 0;
        }
        u8* ret = &memory[index];
        index += size;
        return ret;
    }

    void allocate(size_t size)
    {
        memory.resize(size, 0);
        index = 0;
    }

private:
    std::vector<u8> memory;
    size_t index;
};
