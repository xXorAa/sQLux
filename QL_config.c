/*
 * (c) UQLX - see COPYRIGHT
 */

#include "QL68000.h"
#include "QL.h"
#include <stdio.h>
#include "iexl_general.h"
#include "xcodes.h"
#include "QDOS.h"
#include "QL_basext.h"
#include "QL_config.h"
#include "QL_driver.h"
#include "QL_poll.h"
#include "QL_screen.h"
#include "unix.h"
#include "unixstuff.h"

/*#include "QTimer.h"
#include "QColor.h"
#include "QSound.h"
#include "QDisk.h"
#include "QSerial.h" */
#include <string.h>
/*#include "ConfigDialog.h"*/
#include "QFilesPriv.h"

#include "uqlx_cfg.h"
#include "xqlmouse.h"
#include "Xscreen.h"

#include "m68k.h"
#include "sqlux_hook_pc.h"
#include "sqlux_swap_cpu.h"
#define DEBUG
#include "sqlux_debug.h"
#include "sqlux_memory.h"
#include "sqlux_qdos_traps.h"

static short ramItem = -1;
extern int do_update;

static Cond qemlPatch = false;

int isMinerva = 0;
uw32 orig_kbenc;

int testMinervaVersion(char *ver);

Cond LookFor(uw32 *a, uw32 w, long nMax)
{
	while (nMax-- > 0 && RL((Ptr)theROM + (*a)) != w)
		(*a) += 2;
	return nMax > 0;
}
static Cond LookFor2(uw32 *a, uw32 w, uw16 w1, long nMax)
{
rty:
	while (nMax-- > 0 && RL((Ptr)theROM + (*a)) != w)
		(*a) += 2;
	if (RW((Ptr)theROM + (*a) + 4) == w1)
		return nMax > 0;
	(*a) += 2;
	if (nMax > 0)
		goto rty;
	return 0;
}

static void PatchKeyTrans(void)
{
	KEYTRANS_CMD_ADDR = RL((Ptr)theROM + 0xbff2);
	KEYTRANS_OCODE = RW((uw16 *)((Ptr)theROM + KEYTRANS_CMD_ADDR));
	WW(((uw16 *)((Ptr)theROM + KEYTRANS_CMD_ADDR)), KEYTRANS_CMD_CODE);
}

static Cond PatchFind(void)
{
	Cond ok;

	IPC_CMD_ADDR = 0x2b80;
	IPCR_CMD_ADDR = 0x2e98;
	IPCW_CMD_ADDR = 0x2e78;
	ok = LookFor(&IPC_CMD_ADDR, 0x40e7007c, 2000);
	if (ok)
		ok = LookFor(&IPCR_CMD_ADDR, 0x12bc000e, 2000);
	if (ok)
		ok = LookFor(&IPCW_CMD_ADDR, 0xe9080000, 2000);
	return ok;
}

static Cond InstallQemlRom(void)
{
	if (!qemlPatch) {
		if (isMinerva) {
			ROMINIT_CMD_ADDR = 0x0;
			qemlPatch = LookFor2(&ROMINIT_CMD_ADDR, 0x0c934afbl, 1,
					     48 * 1024);
			if (testMinervaVersion("1.89")) {
				uintptr_t mpatch_addr = 0x4e6;
				uint16_t tmp = RW((Ptr)theROM + mpatch_addr);
				printf("Minerva 1.89, checking for >1MB patch\n");
				if (tmp == 0xd640) {
					WW((Ptr)theROM + mpatch_addr, 0xd680);
					printf("....applied patch\n");
				}
			}
		}

		else /* not Minerva */
		{
			ROMINIT_CMD_ADDR = 0x4a00;
			qemlPatch =
				LookFor(&ROMINIT_CMD_ADDR, 0x0c934afbl, 1000);
		}
		if (!qemlPatch)
			printf("Warning: could not patch ROM scan sequence\n");
		if (qemlPatch) {
			//WW(((uw16 *)((Ptr)theROM + ROMINIT_CMD_ADDR)),
			//   ROMINIT_CMD_CODE);
			hook_callbacks[ROM_INIT_HOOK].addr = ROMINIT_CMD_ADDR;
			WW(((uw16 *)((Ptr)theROM + MDVIO_CMD_ADDR)),
			   MDVIO_CMD_CODE); /* device driver routines */
			WW(((uw16 *)((Ptr)theROM + MDVO_CMD_ADDR)),
			   MDVO_CMD_CODE);
			WW(((uw16 *)((Ptr)theROM + MDVC_CMD_ADDR)),
			   MDVC_CMD_CODE);
			WW(((uw16 *)((Ptr)theROM + MDVSL_CMD_ADDR)),
			   MDVSL_CMD_CODE);
			WW(((uw16 *)((Ptr)theROM + MDVFO_CMD_ADDR)),
			   MDVFO_CMD_CODE);
		}
	}
	return qemlPatch;
}

static Cond ReasonableROM(w32 *r)
{
	short i;
	w16 ut;

	if ((RL(&r[1]) < 0) || (RL(&r[1]) >= 49151) ||
	    ((short)RL(&r[1]) & 1) != 0)
		return false;
	for (i = 3; i < 10; i++)
		if (RL(&r[i]) < 0 || RL(&r[i]) >= 49151 ||
		    ((short)RL(&r[i]) & 1) != 0)
			return false;
	if (RL(&r[26]) < 0 || RL(&r[26]) >= 49151 ||
	    ((short)RL(&r[26]) & 1) != 0)
		return false;
	for (i = 32; i < 48; i++)
		if (RL(&r[i]) < 0 || RL(&r[i]) >= 49151 ||
		    ((short)RL(&r[i]) & 1) != 0)
			return false;
	for (i = 0xc0; i < 0xd6; i += 2) {
		ut = RW((w16 *)((Ptr)r + i));
		if (ut < 0 || (ut & 1) != 0)
			return false;
	}
	for (i = 0xd8; i <= 0x12a; i += 2) {
		ut = RW((w16 *)((Ptr)r + i));
		if (ut < 0 || (ut & 1) != 0)
			return false;
	}
	return true;
}
/* presently simply search for "QView" */
int testMinerva(void)
{
	char *p = (char *)theROM;
	char *limit = (Ptr)theROM + 48 * 1024;
	/* char *limit=(Ptr)theROM+4*1024; */
	/* "JSL1" should be found in 1st 4K */ /* .hpr 8.8.99 */

	do {
		/*       p=memchr(p,'Q',limit-p); */
		/*       if (!p) return 0; */
		/*       if (!memcmp(++p,"View",4)) return 1; */

		p = memchr(p, 'J', limit - p);
		if (!p)
			return 0;
		if (!memcmp(++p, "SL1", 3))
			return 1;
	} while (p && p < limit);

	return 0;
}

int testMinervaVersion(char *ver)
{
	char *p = (char *)theROM;
	char *limit = (Ptr)theROM + 48 * 1024;

	do {
		p = memchr(p, ver[0], limit - p);
		if (!p)
			return 0;
		if (!memcmp(++p, &ver[1], 3))
			return 1;
	} while (p && p < limit);
	return 0;
}

static uint32_t bootpatchaddr[] = {
                    0x842A, /* Minerva 1.89 */
                    0x83CA, /* Minerva 1.98 */
                    0x83CC, /* Minerva 1.98a1 */
                    0x4BE6, /* JS */
                    0 };

static void PatchBootDev()
{
	int i = 0;

	/* patch the boot device in ROM */
	while (bootpatchaddr[i]) {
		if (!strncasecmp((void *)theROM + bootpatchaddr[i], "mdv1",
				 4)) {
			if (V2)
				printf("Patching Boot Device %s at 0x%x\n",
					QMD.bootdev, bootpatchaddr[i]);

			strncpy((void *)theROM + bootpatchaddr[i], QMD.bootdev,
				4);
		}
		i++;
	}
}

short LoadMainRom(void) /* load and modify QL ROM */
{
	short e;
	long l;
	uw32 a;
	Cond p;

	qemlPatch = false;

	p = 1; /*ReasonableROM(theROM);*/

	isMinerva = testMinerva();
	if (isMinerva && V1)
		printf("using Minerva ROM\n");

	//if(V1)printf("no_patch: %d\n",QMD.no_patch);

	if (p && !QMD.no_patch) {
		if (!isMinerva) {
			if (p)
				p = PatchFind();
			if (p) {
				WW((((Ptr)theROM + IPC_CMD_ADDR)),
				   IPC_CMD_CODE);
				WW((((Ptr)theROM + IPCR_CMD_ADDR)),
				   IPCR_CMD_CODE);
				WW((((Ptr)theROM + IPCW_CMD_ADDR)),
				   IPCW_CMD_CODE);
			}
		}

		WW((((Ptr)theROM + 0x4000 + RW((uw16 *)((Ptr)theROM + 0x124)))),
		   MDVR_CMD_CODE); /* read mdv sector */
		WW((((Ptr)theROM + 0x4000 + RW((uw16 *)((Ptr)theROM + 0x126)))),
		   MDVW_CMD_CODE); /* write mdv sector */
		WW(((uw16 *)((Ptr)theROM + 0x4000 +
			     RW((uw16 *)((Ptr)theROM + 0x128)))),
		   MDVV_CMD_CODE); /* verify mdv sector */
		WW(((uw16 *)((Ptr)theROM + 0x4000 +
			     RW((uw16 *)((Ptr)theROM + 0x12a)))),
		   MDVH_CMD_CODE); /* read mdv sector header */

		if (!isMinerva && QMD.fastStartup)
			WW(((uw16 *)((Ptr)theROM + RL(&theROM[1]))),
			   FSTART_CMD_CODE); /* fast startup patch */
		/* FastStartup() -- 0xadc7 */

		if (!isMinerva) {
			/* correct bugs which crashes the QL with 4M ram */
			a = 0x250;
			p = LookFor(&a, 0xd6c028cb, 6);
			if (p) {
				WW(((uw16 *)((Ptr)theROM + a)), 0xd7c0);
				a = 0x3120;
				p = LookFor(&a, 0xd2c02001, 250);
			} else if (V3)
				printf("looks like ROM doesn't have 16MB bugs\n");
			if (p) {
				WW(((uw16 *)((Ptr)theROM + a)), 0xd3c0);
				a = 0x4330;
				p = LookFor(&a, 0x90023dbc, 120);
				if (p)
					WW(((uw16 *)((Ptr)theROM + a)), 0x9802);
			}

			PatchKeyTrans();
		}

		p = InstallQemlRom();

		PatchBootDev();
	}
	if (!p && !QMD.no_patch)
		printf("warning : could not complete ROM patch\n");

	/* last not least intrument the ROM code HW register access */

	if (e == 0 && !p)
		e = ERR_ROM_UNKNOWN;
	return e;
}

void save_regs(void *p)
{
	memcpy(p, reg, 14 * 4);
}
void restore_regs(void *p)
{
	memcpy(reg, p, 14 * 4);
}

extern int tracetrap;

void InitROM(void)
{
	char qvers[6];
	char *initstr = "UQLX v%s, release\012      %s\012QDOS Version %s\012";
	uint32_t sysvars, sxvars, alcmem;
	unsigned int pc, pc_inst;
	int sub1, sub2;

	DEBUG_PRINT("Entered\n");

	pc = m68k_get_reg(NULL, M68K_REG_PC);
	pc_inst = ReadWord(pc);

	if (pc_inst != 0x0C93) {
		fprintf(stderr, "InitROM: reached from incorrect instruction %x,%x\n", pc, pc_inst);
		exit(1);
	}

	sqlux_store_main_ctx();

	sqlux_trap(1, MT_INF);

	/* Standard systems vars */
	sysvars = m68k_get_reg(NULL, M68K_REG_A0);

	/* delete old MDV drivers (for optical reasons) */
	WriteLong(sysvars + 0x48, 0);

	InitFileDrivers();
	//InitDrivers();

	//init_xscreen();

	//SchedInit();

	//init_bas_exts();


	/* Minerva extended vars */
	sxvars = ReadLong(sysvars + 0x7c);

	if (V3)
		printf("sysvars at %x, ux RAMTOP %d, sys.ramt %d, qlscreen at %d\n",
		       (unsigned)sysvars, RTOP, sysvar_l(20), qlscreen.qm_lo);

	// QDOS version
	WL((Ptr)qvers, m68k_get_reg(NULL, M68K_REG_D2));
	qvers[4] = 0;

	if (V3) printf("QDOS version %s\n", qvers);

	if (strncmp(qvers, "1.98", 4) == 0) {
		sub1 = ReadByte(sxvars + 74);
		if (sub1 == 'a') {
			sub2 = ReadByte(sxvars + 75);
			if (V3) printf("QDOS subversion %c%c\n", sub1, sub2);
		}
	}

	/* now install TKII defaults */

	/* allocate 0x6c bytes, jobid 0 */
	m68k_set_reg(M68K_REG_D1, 0x6c);
	m68k_set_reg(M68K_REG_D2, 0);

	sqlux_trap(1, MT_ALCHP);

	if ( m68k_get_reg(NULL, M68K_REG_D0) == 0) {
		alcmem = m68k_get_reg(NULL, M68K_REG_A0);
		if (V3)
			printf("Initialising TK2 device defaults\n");
		WriteLong(sysvars + 0x70 + 0x3c, alcmem);
		WriteLong(sysvars + 0x70 + 0x40, 32 + alcmem);
		WriteLong(sysvars + 0x70 + 0x44, 64 + alcmem);
		WriteWord(alcmem, strlen(DATAD));
		strcpy((char *)m68k_to_host(alcmem + 2), DATAD);
		WriteWord(alcmem + 32, strlen(PROGD));
		strcpy((char *)m68k_to_host(alcmem + 34), PROGD);
		WriteWord(alcmem + 64, strlen(SPOOLD));
		strcpy((char *)m68k_to_host(alcmem + 66), SPOOLD);
	} else {
		fprintf(stderr, "InitROM tk2 memory allocation failure %x\n",
				m68k_get_reg(NULL, M68K_REG_D0));
	}

	/* link in Minerva keyboard handling */
	if (isMinerva) {
		/* allocate 0x6c bytes, jobid 0 */
		m68k_set_reg(M68K_REG_D1, 0x08);
		m68k_set_reg(M68K_REG_D2, 0);

		sqlux_trap(1, MT_ALCHP);

		if (m68k_get_reg(NULL, M68K_REG_D0) == 0) {
			alcmem = m68k_get_reg(NULL, M68K_REG_A0);
			hook_callbacks[ROM_MIPC_CMD].addr = MIPC_CMD_ADDR;
			WriteLong(alcmem, ReadLong(sxvars + 0x14));
			WriteLong(alcmem + 4, MIPC_CMD_ADDR);
			WriteLong(sxvars + 0x14, alcmem);
		} else {
			fprintf(stderr,
				"InitROM minerva memory allocation failure %x\n",
				m68k_get_reg(NULL, M68K_REG_D0));
		}

		WW((Ptr)theROM + KBENC_CMD_ADDR, KBENC_CMD_CODE);
		orig_kbenc = RL((Ptr)theROM + sxvars + 0x10);
		WL((Ptr)theROM + sxvars + 0x10, KBENC_CMD_ADDR);
	}

	init_poll();

	sqlux_restore_main_ctx();

	DEBUG_PRINT("Exited\n");
}

#if 0
static short LoadRoms(void)
{	short e;

	e=LoadMainRom();
	if(e==0) e=InstallBackRom();
	if(e==fnfErr) e=ERR_ROM;
	return e;
}
#endif /*0*/

static void RestartQL(void)
{
	short i;
	CloseAllFiles();
	for (i = 0; i < 15; i++)
		reg[i] = 0;
	memset((Ptr)theROM + 131072l, 0, RTOP - 131072l);
	InitialSetup();
}

#if 0
static short StartQL(void)
{	short e;

	e=QL_memory();
	if(e==0)
	{	e=LoadRoms();
		if(e==0)
		{	InitSerial();
			RestartQL();
			e=AllocateDisk();
			if(e==0) e=AllocateSound();
			if(e==0)
			{	qlRunning=true;
				StartTimer();
			}
			else DisposePtr((Ptr)theROM);
		}
		else DisposePtr((Ptr)theROM);
	}
	return e;
}
#endif
