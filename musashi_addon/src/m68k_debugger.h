#ifndef _M68K_DEBUGGER_H_
#define _M68K_DEBUGGER_H_

#include "m68k_elf_loader.h"
#include "core/m68kcpu.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct m68k_debugger
{
	int state;
} m68k_debugger;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_debugger_init();
int m68k_debugger_update();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern struct m68k_debugger* g_debugger;

#endif
