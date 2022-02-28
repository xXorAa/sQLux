/*
 * (c) UQLX - see COPYRIGHT
 */

/* hook for QDOS poll routine */


/*#include "QLtypes.h"*/
#include "instructions.h"
#include "QL68000.h"
#include "xcodes.h"
#include "QL.h"
#include "QDOS.h"
#include "QInstAddr.h"
#include "QL_hardware.h"
#include "unix.h"

#include "m68k.h"
#include "sqlux_memory.h"
#include "sqlux_qdos_traps.h"

/*extern int schedCount;*/
extern volatile int poll_req;

#if 0
extern uw8	IPCR_buff[22];
extern uw16	IPCR_ascii[22];
extern short IPCR_n;
extern short IPCR_p;

extern uw16 asciiChar;
#endif

void init_poll()
{
	void *p;
  //reg[1]=0x10;
  //reg[2]=0;
  //QLtrap(1,0x18,2000000l);
	m68k_set_reg(M68K_REG_D1, 0x10);
	m68k_set_reg(M68K_REG_D2, 0);
	sqlux_trap(1, MT_ALCHP);

  	if (m68k_get_reg(NULL, M68K_REG_D0) == 0)
    	{
      		//Ptr p=(Ptr)theROM+aReg[0];
		p = m68k_to_host(m68k_get_reg(NULL, M68K_REG_A0));
		p = p + 4;

		WL( p, POLL_CMD_ADDR);
		WW((Ptr)theROM+POLL_CMD_ADDR, POLL_CMD_CODE);

		//QLtrap(1,0x1c,200000l);

		sqlux_trap(1, MT_LPOLL);
	}
}


void PollCmd()
{
  if((Ptr)gPC-(Ptr)theROM-2!=POLL_CMD_ADDR)
    {
      exception=4;
      extraFlag=true;
      nInst2=nInst;
      nInst=0;
      return;
    }

#ifdef VTIME
  if (poll_req>1)
    printf("poll_req=%d in PollCmd()\n",poll_req);
  poll_req=0;
#endif

  if (isMinerva)
    MReadKbd();

  rts();
}


