#pragma once

union CoreConfiguration {
    struct {
        bool use_cpu_recompiler;
        bool use_rsp_recompiler;
    } n64;
};
