#include <stdlib.h>

#include "m68k.h"

void *main_ctx;
void *trap_ctx;

void sqlux_allocate_main_ctx(void)
{
	main_ctx = calloc(1, m68k_context_size());
}

void sqlux_allocate_trap_ctx(void)
{
	trap_ctx = calloc(1, m68k_context_size());
}

void sqlux_store_main_ctx(void)
{
	m68k_get_context(main_ctx);
}

void sqlux_store_trap_ctx(void)
{
	m68k_get_context(trap_ctx);
}

void sqlux_restore_main_ctx(void)
{
	m68k_set_context(main_ctx);
}

void sqlux_restore_trap_ctx(void)
{
	m68k_set_context(trap_ctx);
}

