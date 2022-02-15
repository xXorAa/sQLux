#pragma once

#include "m68k.h"

extern void *main_ctx;
extern void *trap_ctx;

void sqlux_allocate_main_ctx(void);
void sqlux_allocate_trap_ctx(void);
void sqlux_store_main_ctx(void);
void sqlux_store_trap_ctx(void);
void sqlux_restore_main_ctx(void);
void sqlux_restore_trap_ctx(void);

