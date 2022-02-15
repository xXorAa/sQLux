/*
 * (c) UQLX - see COPYRIGHT
 */


/* define memory access fns */
#include "QL68000.h"
#include "memaccess.h"
#include "general.h"
#include "QL_screen.h"
#include "SDL2screen.h"
#include "sqlux_trap.h"

void *m68k_memspace;

static int is_screen(uint32_t addr)
{
	if ((addr >= qlscreen.qm_lo) && (addr < qlscreen.qm_hi)) {
		return 1;
	}
	return 0;
}

static int is_hw(uint32_t addr)
{
	if ((addr >= QL_INTERNAL_IO_BASE) &&
        	(addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
		return 1;
	}

	if ((addr >= QL_EXTERNAL_IO_BASE) &&
		( addr < (QL_EXTERNAL_IO_BASE + QL_EXTERNAL_IO_SIZE))) {
		return 1;
	}

	return 0;
}

unsigned int ReadByte(unsigned int addr)
{
	addr &= ADDR_MASK;

	if ((addr >= RTOP) && (addr >=qlscreen.qm_hi))
		return 0;

	if (is_hw(addr)) {
		return ReadHWByte(addr);
	}

	return *(uint8_t *)(m68k_memspace + addr);
}

unsigned int ReadWord(unsigned int addr)
{
	addr &= ADDR_MASK;

	if (sqlux_in_trap) {
		switch (addr) {
		case 0:
			printf("ins %x\n", 0x4e40 | sqlux_trap_no);
			return 0x4e40 | sqlux_trap_no;
			break;
		case 2:
			printf("ins %x\n", 0x4e71);
			return 0x4e71;
			break;
		}
	}

	if ((addr >= RTOP) && (addr >=qlscreen.qm_hi))
		return 0;

	if (is_hw(addr)) {
		return ((w16)ReadHWWord(addr));
	}

	//printf("ReadWord %x:%x\n", addr, SDL_SwapBE16(*(uint16_t *)(m68k_memspace + addr)));
	return SDL_SwapBE16(*(uint16_t *)(m68k_memspace + addr));
}

unsigned int ReadLong(unsigned int addr)
{
	addr &= ADDR_MASK;

	if ((addr >= RTOP) && (addr >=qlscreen.qm_hi))
		return 0;

	if (is_hw(addr)) {
		return ((w32)ReadHWLong(addr));
	}

	return SDL_SwapBE32(*(uint32_t *)(m68k_memspace + addr));
}

void WriteByte(unsigned int addr, unsigned int d)
{
	addr &= ADDR_MASK;

	if ((addr >= RTOP) && (addr >= qlscreen.qm_hi))
		return;

	if (is_screen(addr)) {
		*((w8 *)theROM + addr) = d;
		QLSDLUpdateScreenByte(addr - qlscreen.qm_lo, d);
	} else if (is_hw(addr)) {
		WriteHWByte(addr, d);
	} else if (addr >= QL_ROM_SIZE) {
		*((w8 *)theROM + addr) = d;
	}
}

void WriteWord(unsigned int addr, unsigned int d)
{
	addr &= ADDR_MASK;

	if ((addr >= RTOP) && (addr >= qlscreen.qm_hi))
		return;

	if (is_screen(addr)) {
		WW((Ptr)theROM + addr, d);
		QLSDLUpdateScreenWord(addr - qlscreen.qm_lo, d);
	} else if (is_hw(addr)) {
		WriteHWWord(addr, d);
	} else if (addr >= QL_ROM_SIZE) {
		WW((Ptr)theROM + addr, d);
	}
}

void WriteLong(unsigned int addr, unsigned int d)
{
	addr &= ADDR_MASK;

	if ((addr >= RTOP) && (addr >=qlscreen.qm_hi))
		return;

	if (is_screen(addr)) {
		WL((Ptr)theROM + addr, d);
		QLSDLUpdateScreenLong(addr - qlscreen.qm_lo, d);
	} else if (is_hw(addr)) {
		WriteHWWord(addr, d >> 16);
		WriteHWWord(addr + 2, d);
	} else if (addr >= QL_ROM_SIZE) {
		WL((Ptr)theROM + addr, d);
	}
}

/*############################################################*/
int isreg=0;

rw8 ModifyAtEA_b(ashort mode,ashort r)
{
	shindex displ;
	w32 addr;

	isreg = 0;

	switch (mode)
	{
	case 0:
		isreg = 1;
		mea_acc = 0;
		lastAddr = 0;
		dest = (Ptr)(&reg[r]) + RBO;
		return *((w8 *)dest);
	case 2:
		isreg = 1;
		addr = aReg[r];
		break;
	case 3:
		addr = aReg[r]++;
		if (r == 7)
			(*m68k_sp)++;
		break;
	case 4:
		if (r == 7)
			(*m68k_sp)--;
		addr = --aReg[r];
		break;
	case 5:
		addr = aReg[r] + (w16)RW(pc++);
		break;
	case 6:
		displ = (w16)RW(pc++);
		if ((displ & 2048) != 0)
			addr = reg[(displ >> 12) & 15] +
			       aReg[r] + (w32)((w8)displ);
		else
			addr = (w32)((w16)(reg[(displ >> 12) & 15])) +
			       aReg[r] + (w32)((w8)displ);
		break;
	case 7:
		switch (r)
		{
		case 0:
			addr = (w16)RW(pc++);
			break;
		case 1:
			addr = RL((w32 *)pc);
			pc += 2;
			break;
		default:
			exception = 4;
			extraFlag = true;
			nInst2 = nInst;
			nInst = 0;

			mea_acc = 0;
			lastAddr = 0;
			dest = (Ptr)(&dummy);
			return 0;
		}
		break;
	default:
		exception = 4;
		extraFlag = true;
		nInst2 = nInst;
		nInst = 0;

		mea_acc = 0;
		lastAddr = 0;
		dest = (Ptr)(&dummy);
		return 0;
	}

	addr &= ADDR_MASK;

	lastAddr = addr;
	dest = (Ptr)theROM + addr;
	return ReadByte(addr);
}

rw16 ModifyAtEA_w(ashort mode,ashort r)
{
	/*w16*/
	shindex displ;
	w32 addr = 0;

	isreg = 0;

	switch (mode)
	{
	case 0:
		isreg = 1;
		dest = (Ptr)(&reg[r]) + RWO;
		return *((w16 *)dest);
	case 1:
		isreg = 1;
		dest = (Ptr)(&aReg[r]) + RWO;
		return *((w16 *)dest);
	case 2:
		addr = aReg[r];
		break;
	case 3:
		addr = aReg[r];
		aReg[r] += 2;
		break;
	case 4:
		addr = (aReg[r] -= 2);
		break;
	case 5:
		addr = aReg[r] + (w16)RW(pc++);
		break;
	case 6:
		displ = (w16)RW(pc++);
		if ((displ & 2048) != 0)
			addr = reg[(displ >> 12) & 15] +
			       aReg[r] + (w32)((w8)displ);
		else
			addr = (w32)((w16)(reg[(displ >> 12) & 15])) +
			       aReg[r] + (w32)((w8)displ);
		break;
	case 7:
		switch (r)
		{
		case 0:
			addr = (w16)RW(pc++);
			break;
		case 1:
			addr = RL((w32 *)pc);
			pc += 2;
			break;
		default:
			exception = 4;
			extraFlag = true;
			nInst2 = nInst;
			nInst = 0;
			mea_acc = 0;
			dest = (Ptr)(&dummy);
			return 0;
		}
		break;
	}
	addr &= ADDR_MASK;

	lastAddr = addr;
	dest = (Ptr)theROM + addr;
	return ReadWord(addr);
}

rw32 ModifyAtEA_l(ashort mode, ashort r)
{
	/*w16*/
	shindex displ;
	w32 addr = 0;

	isreg = 0;

	switch (mode)
	{
	case 0:
		isreg = 1;
		dest = (Ptr)(&reg[r]);
		return *((w32 *)dest);
	case 1:
		isreg = 1;
		dest = (Ptr)(&aReg[r]);
		return *((w32 *)dest);
	case 2:
		addr = aReg[r];
		break;
	case 3:
		addr = aReg[r];
		aReg[r] += 4;
		break;
	case 4:
		addr = (aReg[r] -= 4);
		break;
	case 5:
		addr = aReg[r] + (w16)RW(pc++);
		break;
	case 6:
		displ = (w16)RW(pc++);
		if ((displ & 2048) != 0)
			addr = reg[(displ >> 12) & 15] +
			       aReg[r] + (w32)((w8)displ);
		else
			addr = (w32)((w16)(reg[(displ >> 12) & 15])) +
			       aReg[r] + (w32)((w8)displ);
		break;
	case 7:
		switch (r)
		{
		case 0:
			addr = (w16)RW(pc++);
			break;
		case 1:
			addr = RL((w32 *)pc);
			pc += 2;
			break;
		default:
			exception = 4;
			extraFlag = true;
			nInst2 = nInst;
			nInst = 0;
			mea_acc = 0;

			dest = (Ptr)(&dummy);
			return 0;
		}
		break;
	}

	addr &= ADDR_MASK;

	lastAddr = addr;
	dest = (Ptr)theROM + addr;
	return ReadLong(addr);
}

void RewriteEA_b(aw8 d)
{
	if (isreg)
		*((w8*)dest)=d;
	else {
		WriteByte(lastAddr, d);
	}
}

void RewriteEA_w(aw16 d)
{
	if (isreg) {
		*((w16*)dest)=d;
	} else {
		WriteWord(lastAddr, d);
	}
}

void RewriteEA_l(aw32 d)
{
	if (isreg) {
		*((w32*)dest)=d;
	} else {
		WriteLong(lastAddr, d);
	}
}
