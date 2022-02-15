#pragma once

#include "m68k.h"

extern int sqlux_in_trap;
extern int sqlux_trap_no;

void sqlux_trap(int trapno, int d0);

