#pragma once

typedef struct {
    uint32_t addr;
    void (*func)(void);
} hook_callbacks_t;

enum hook_codes {
    ROM_INIT_HOOK,
    ROM_MIPC_CMD,
    ROM_MAX_HOOK
};

/* fixed trap addresses */
#define MIPC_CMD_ADDR       0x14032

extern hook_callbacks_t hook_callbacks[ROM_MAX_HOOK];

