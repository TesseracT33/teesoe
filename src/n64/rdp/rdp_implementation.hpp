#pragma once

#include "frontend/render_context.hpp"
#include "numtypes.hpp"
#include "status.hpp"

struct SDL_Window;

namespace n64 {

class RdpImplementation {
public:
    virtual ~RdpImplementation() = default;

    virtual void EnqueueCommand(int cmd_len, u32* cmd_ptr) = 0;
    virtual void OnFullSync() = 0;
    virtual void UpdateScreen() = 0;
};

} // namespace n64
