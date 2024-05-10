#pragma once

#include "status.hpp"
#include "numtypes.hpp"

namespace n64 {

class RDPImplementation {
public:
    virtual ~RDPImplementation() = default;

    virtual void EnqueueCommand(int cmd_len, u32* cmd_ptr) = 0;
    virtual Status Initialize() = 0;
    virtual void OnFullSync() = 0;
    virtual void UpdateScreen() = 0;
};

} // namespace n64
