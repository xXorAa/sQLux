#include <stdio.h>
#include <stdint.h>

#include "m68k.h"
#include "sqlux_hook_pc.h"

void InitROM(void);

static char disass[1024];

static int init_rom = 0;

hook_callbacks_t hook_callbacks[ROM_MAX_HOOK] = {
[ROM_INIT_HOOK] = { 0x8292, InitROM },
};

void sqlux_hook_pc(unsigned int pc)
{
	int i;

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
