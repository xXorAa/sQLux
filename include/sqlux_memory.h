#pragma once

#define m68k_to_host(a) ((void *)theROM + (a))
#define host_to_m68k(a) ((void *)(a) - (void *)theROM)

