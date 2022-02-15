#include <stdio.h>
#include <stdint.h>

#include "m68k.h"
#include "SDL2screen.h"
#include "sqlux_hook_pc.h"
#include "sqlux_trap.h"
#include "uqlx_cfg.h"

void InitROM(void);

static char disass[1024];

static int init_rom = 0;

hook_callbacks_t hook_callbacks[ROM_MAX_HOOK] = {
[ROM_INIT_HOOK] = { 0x8292, InitROM },
};

void sqlux_hook_pc(unsigned int pc)
{
	int i;

	/* trigger the 50Hz IRQ */
	if (SDL_AtomicGet(&doPoll)) {
		dosignal();
		m68k_set_irq(2);
	}

	if (sqlux_in_trap && (pc == 0))
		printf("trapping\n");
	if (sqlux_in_trap && (pc == 2)) {
		printf("Ending Timeslize\n");
		m68k_end_timeslice();
	}

	if (sqlux_in_trap)
		printf("PC: %x a0 %x\n", pc, m68k_get_reg(NULL, M68K_REG_A0));

	if (QMD.no_patch)
		return;

	for (i = 0; i < ROM_MAX_HOOK; i++) {
		if (hook_callbacks[i].addr == pc) {

			/* Only call RomINIT once */
			if (i == ROM_INIT_HOOK) {
			       hook_callbacks[i].addr = 0xFFFFFFFF;
			}

			hook_callbacks[i].func();
		}
	}

//	printf("PC: %x\n", pc);

//	m68k_disassemble(disass, pc, M68K_CPU_TYPE_68000);
//	printf("%s\n", disass);
}
