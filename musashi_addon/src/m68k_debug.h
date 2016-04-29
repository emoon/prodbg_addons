#ifndef _M68K_DEBUG_H_
#define _M68K_DEBUG_H_

#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct M68KBreakpoint
{
	char filename[2048];
	int line;
	uint32_t pc;
	int id;

} M68KBreakpoint;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_add_breakpoint(const char* file, int line);
int m68k_del_breakpoint(uint32_t id);
M68KBreakpoint* m68k_get_breakpoints(uint32_t* count);
int m68k_disasm_pc(char* outputBuffer, int outputBufferSize, int pc, int* instCount);

#endif

