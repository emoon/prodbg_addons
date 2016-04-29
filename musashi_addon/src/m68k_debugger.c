#include "m68k_elf_loader.h"
#include "m68k_debugger.h"
#include <pd_backend.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "m68k.h"
#include "m68kcpu.h"

void m68k_disasm_function(int length);
extern void m68k_execute_single_instruction();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_debugger_init()
{
	void* m68k_data = malloc(12 * 1024 * 1024);
	int sizes = 4 * 1024 * 1024;

	m68k_code_init(m68k_data, sizes, sizes, sizes);

	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);

	m68ki_jump(0);

	// set the the stack pointer and push the return start address on it so when the function is finished
	// the sp will point at the top of the stack

	m68ki_set_sp(0x10000);
	m68ki_push_32(0);

	m68k_elf_load("/Users/danielcollin/temp/test.elf");

	// always run from 0
	// m68ki_jump(pc);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_disasm_function(int length)
{
	int i = 0, pc = 0; //(int)m68k_find_symbol(func);

	/*
	if (pc < 0)
	{
		m68k_log(M68K_LOG_ERROR, "Unable to find function: '%s' code will not be called\n", func);
		return;
	}
	*/

	while (1)
	{
		int inst_size;
		char instructionText[256];

        memset(instructionText, 0, sizeof(instructionText));
		inst_size = m68k_disassemble(instructionText, pc, M68K_CPU_TYPE_68000);

		printf("%08x: %s\n", pc, instructionText);

		i += inst_size;
		pc += inst_size;

		if (i >= length)
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool m68k_debugger_update()
{
    if (!g_debugger)
    	return true;

	switch (g_debugger->state)
	{
		case PDDebugState_Running :
		{
			// execute 100 instructions if we are running

			for (int i = 0; i < 100; ++i)
				m68k_execute_single_instruction();

			return true;
		}

		case PDDebugState_Trace :
		{
			m68k_execute_single_instruction();
			g_debugger->state = PDDebugState_StopException;
			printf("step singe\n");
			break;
		}
	}

	return true;
}

