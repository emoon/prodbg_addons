#include <pd_backend.h>
#include <pd_readwrite.h>
#include <stdlib.h>
#include "m68k_debugger.h"
#include "m68k_debug.h"
#include "m68kcpu.h"
#include "m68k_elf_loader.h"
#include <string.h>
#include <stdio.h>

m68k_debugger* g_debugger = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* createInstance(ServiceFunc* serviceFunc)
{
	(void)serviceFunc;

	g_debugger = malloc(sizeof(m68k_debugger));	// this is a bit ugly but for this plugin we only have one instance
	//g_debugger->state = PDDebugState_Running;
	g_debugger->state = PDDebugState_StopException;

	m68k_debugger_init();

	return g_debugger;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void destroyInstance(void* userData)
{
	free(userData);
	g_debugger = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setExceptionLocation(PDWriter* writer)
{
	const char* function;
	const char* filename;
	uint32_t line;
	int pc = REG_PC;

	PDWrite_event_begin(writer,PDEventType_SetExceptionLocation);

	// first try to resolve this to a file + line and if we can't set the address (and the debugger will
	// request disassebly (if needed)

	//if (false)
	if (m68k_resolve_file_line(&filename, &line, &function, pc))
	{
		PDWrite_string(writer, "filename", filename);
		PDWrite_u32(writer, "line", line);

		printf("M68KDebugger setLocation %s:%d (%s)\n", filename, line, function);
	}
	else
	{
		printf("M68KDebugger setLocation %08x\n", pc);

		PDWrite_u32(writer, "address", pc);
		PDWrite_u8(writer, "address_size", 4);
	}

	PDWrite_event_end(writer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setDisassembly(PDWriter* writer, int pc, int inst_count)
{
  	PDWrite_event_begin(writer, PDEventType_SetDisassembly);
    PDWrite_array_begin(writer, "disassembly");

	for (int i = 0; i < inst_count; ++i) {
		char text[256] = { 0 };

		int inst_size = m68k_disassemble(text, pc, M68K_CPU_TYPE_68000);

        PDWrite_array_entry_begin(writer);
        PDWrite_u32(writer, "address", pc);
        PDWrite_string(writer, "line", text);
        PDWrite_array_entry_end(writer);

        pc += inst_size;
    }

    PDWrite_array_end(writer);
    PDWrite_event_end(writer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void writeDAregisters(PDWriter* writer, char* name, uint32_t* regs)
{
	int i;

	char tempName[64];
	strcpy(tempName, name);

	for (i = 0; i < 8; ++i)
	{
		PDWrite_array_entry_begin(writer);
		PDWrite_u32(writer, "register", regs[i]);
		PDWrite_string(writer, "name", tempName);
		PDWrite_array_entry_end(writer);

		// some trickery here (name is always in dX format so d0,d1,d2.. etc and a0,a1,a2...)
		tempName[1] += 1;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setRegisters(PDWriter* writer)
{
	PDWrite_event_begin(writer, PDEventType_SetRegisters);
	PDWrite_array_begin(writer, "registers");

	writeDAregisters(writer, "d0", (uint32_t*)&m68ki_cpu.dar[0]);
	writeDAregisters(writer, "a0", (uint32_t*)&m68ki_cpu.dar[8]);

	// PC

	PDWrite_array_entry_begin(writer);
	PDWrite_u32(writer, "register", REG_PC);
	PDWrite_u8(writer, "read_only", 1);
	PDWrite_string(writer, "name", "pc");
	PDWrite_array_entry_end(writer);

	// Flags

	PDWrite_array_entry_begin(writer);
	PDWrite_u8(writer, "flags", 1);
	PDWrite_u32(writer, "register", (FLAG_X << 4) | (FLAG_N << 3) | (FLAG_Z << 2) | (FLAG_V << 1) | (FLAG_C << 0));
	PDWrite_string(writer, "name", "XNZVC");
	PDWrite_array_entry_end(writer);

	PDWrite_array_end(writer);
	PDWrite_event_end(writer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setMemory(PDWriter* writer, uint32_t start, uint32_t end)
{
	uint8_t* startMemory = m68k_get_memory(start);
	uint32_t size = end - start;

	if (end > start)
		PDWrite_data(writer, "memory", startMemory, size);
	else
		printf("start 0x%08x > end 0x%08x, unable to write memory\n", start, end);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void getDisassembly(PDReader* reader, PDWriter* writer)
{
	uint32_t start = 0;
	uint32_t instCount = 1;

	PDRead_find_u32(reader, &start, "address_start", 0);
	PDRead_find_u32(reader, &instCount, "instruction_count", 0);

	setDisassembly(writer, start, instCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void getMemory(PDReader* reader, PDWriter* writer)
{
	uint32_t start = 0;
	uint32_t end = 0;

	PDRead_find_u32(reader, &start, "address_start", 0);
	PDRead_find_u32(reader, &end, "address_end", 0);

	setMemory(writer, start, end);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sendState(PDWriter* writer)
{
	setExceptionLocation(writer);
	setRegisters(writer);		// sending all registers is really not needed
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_action(m68k_debugger* debugger, PDAction action, PDWriter* writer)
{
	int t = (int)action;

	switch (t)
	{
		case PDAction_Break :
		{
			// On this target we can anways break so just set that we have stopped on breakpoint

			printf("break\n");
			debugger->state = PDDebugState_StopException;
			sendState(writer);
			break;
		}

		case PDAction_Run :
		{
			// on this target we can always start running directly again
			printf("run\n");
			debugger->state = PDDebugState_Running;
			break;
		}

		case PDAction_Step :
		{
			// on this target we can always stepp
			printf("step\n");
			debugger->state = PDDebugState_Trace;
			sendState(writer);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setBreakpoint(PDReader* reader)
{
	const char* filename;
	uint32_t line;

	PDRead_find_string(reader, &filename, "filename", 0);
	PDRead_find_u32(reader, &line, "line", 0);

	m68k_add_breakpoint(filename, (int)line);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static PDDebugState update(void* userData, PDAction action, PDReader* reader, PDWriter* writer)
{
	int event = 0;
	m68k_debugger* debugger = (m68k_debugger*)userData;

	m68k_debugger_update();

	// if we have breakpoint set from the outside, send the state and put us into trace mode

	do_action(debugger, action, writer);

	if (debugger->state == PDDebugState_StopBreakpoint)
	{
		sendState(writer);
		debugger->state = PDDebugState_StopException;
		return debugger->state;;
	}

	while ((event = PDRead_get_event(reader)))
	{
		switch (event)
		{
			case PDEventType_GetDisassembly : getDisassembly(reader, writer); break;
			case PDEventType_GetMemory : getMemory(reader, writer); break;
			case PDEventType_SetBreakpoint : setBreakpoint(reader); break;
		}
	}

	return debugger->state;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PDBackendPlugin s_debuggerPlugin =
{
	"Musashi",
    createInstance,
    destroyInstance,
    0,
    update,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PD_EXPORT void InitPlugin(RegisterPlugin* registerPlugin, void* private_data) {
    registerPlugin(PD_BACKEND_API_VERSION, &s_debuggerPlugin, private_data);
}

