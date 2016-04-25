#include "m68k_debug.h"
#include "m68k_debugger.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "m68k_log.h"
#include "m68k_elf_loader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pd_backend.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68KBreakpoint s_breakpoints[512];
static uint32_t s_breakpointCount = 0;
static uint32_t s_breakpointId = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_add_breakpoint(const char* file, int line)
{
	M68KBreakpoint* breakpoint;
	int id;
	uint32_t pc;

	if (!m68k_resolve_pc_line_file(&pc, file, line))
	{
		printf("Unable to add breakpoint (%s:%d) - unable to resolve pc\n", file, line);
		return -1;
	}

	breakpoint = &s_breakpoints[s_breakpointCount++];

	strcpy(breakpoint->filename, file);
	breakpoint->pc = pc;
	breakpoint->line = line;
	breakpoint->id = id = s_breakpointId++;

	return id;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_del_breakpoint(uint32_t id)
{
	int i, count = s_breakpointCount;

	for (i = 0; i < count; ++i)
	{
		M68KBreakpoint* bp = &s_breakpoints[i];
		if (bp->id == id)
		{
			// swap with the last

			s_breakpoints[i] = s_breakpoints[count-1];
			s_breakpointCount--;
			return 1;
		}
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

M68KBreakpoint* m68k_get_breakpoints(uint32_t* count)
{
	*count = s_breakpointCount;
	return s_breakpoints;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define READ_BYTE(BASE, ADDR) (BASE)[ADDR]
#define READ_WORD(BASE, ADDR) (((BASE)[ADDR]<<8) |			\
							  (BASE)[(ADDR)+1])
#define READ_LONG(BASE, ADDR) (((BASE)[ADDR]<<24) |			\
							  ((BASE)[(ADDR)+1]<<16) |		\
							  ((BASE)[(ADDR)+2]<<8) |		\
							  (BASE)[(ADDR)+3])

#define WRITE_BYTE(BASE, ADDR, VAL) (BASE)[ADDR] = (VAL)&0xff
#define WRITE_WORD(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>8) & 0xff;		\
									(BASE)[(ADDR)+1] = (VAL)&0xff
#define WRITE_LONG(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>24) & 0xff;		\
									(BASE)[(ADDR)+1] = ((VAL)>>16)&0xff;	\
									(BASE)[(ADDR)+2] = ((VAL)>>8)&0xff;		\
									(BASE)[(ADDR)+3] = (VAL)&0xff

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern unsigned char* g_68kmem;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reading of program memory (needs to be endian swapped)

unsigned int m68k_read_memory_prog_8(unsigned int address)
{
	return READ_BYTE((uint8_t*)(uintptr_t)address, 0);
}

unsigned int m68k_read_memory_prog_16(unsigned int address)
{
	return READ_WORD((uint8_t*)(uintptr_t)address, 0);
}

unsigned int m68k_read_memory_prog_32(unsigned int address)
{
	return READ_LONG((uint8_t*)(uintptr_t)address, 0);
}

unsigned int m68k_read_disassembler_8  (unsigned int address)
{
	return READ_BYTE((uint8_t*)(uintptr_t)address, 0);
}
unsigned int m68k_read_disassembler_16 (unsigned int address)
{
	return READ_WORD((uint8_t*)(uintptr_t)address, 0);
}

unsigned int m68k_read_disassembler_32 (unsigned int address)
{
	return READ_LONG((uint8_t*)(uintptr_t)address, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reading of regular memory we read in native format

unsigned int m68k_read_memory_8(unsigned int address)
{
	return *((uint8_t*)(uintptr_t)address);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	return *((unsigned short*)((uint8_t*)(uintptr_t)address));
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	return *((unsigned int*)((uint8_t*)(uintptr_t)address));
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (address == 0 || address == 1)
    {
        g_debugger->state = PDDebugState_StopException;
        return;
    }

	*((uint8_t*)(uintptr_t)address) = (unsigned char)value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	*((unsigned short*)((uint8_t*)(uintptr_t)address)) = (unsigned short)value;
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	*((unsigned int*)((uint8_t*)(uintptr_t)address)) = value;
}


