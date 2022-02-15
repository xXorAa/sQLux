#include "m68k.h"
#define DEBUG
#include "sqlux_debug.h"
#include "sqlux_swap_cpu.h"

int sqlux_in_trap = 0;
int sqlux_trap_no = 0;

void sqlux_trap(int trapno, int d0)
{
	int i;

	DEBUG_PRINT("Starting TRAP %d\n", trapno);

	m68k_set_reg(M68K_REG_PC, 0);

	sqlux_in_trap = 1;
	sqlux_trap_no = trapno;

	m68k_execute(100000);

	sqlux_in_trap = 0;

	DEBUG_PRINT("Stoping TRAP %d\n", trapno);
}

