#pragma once

typedef struct {
    uint32_t addr;
    void (*func)(void);
} hook_callbacks_t;

enum hook_codes {
    ROM_INIT_HOOK,
    ROM_MAX_HOOK
};

extern hook_callbacks_t hook_callbacks[ROM_MAX_HOOK];

