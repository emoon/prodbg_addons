#pragma once

#include <stdint.h>
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get label for a range of memory

typedef struct M68KLabelAddress
{
	const char* name;
	uint32_t address;

} M68KLabelAddress;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// load elf file (68K format only supported)
// Notice that if a file already has been loaded it will replace the existing one

int m68k_elf_load(const char* filename);

// Links all the loaded elf files and reallocs them

bool m68k_elf_link();

// init of the code loading and such

int m68k_code_init(void* memory, int codeSize, int dataSize, int bssSize);

// Find the pc for the function with a given name

int m68k_find_symbol(const char* symbol);

void m68k_find_labels(M68KLabelAddress* labels, uint32_t* count, uint32_t pcStart, uint32_t pcEnd);

// Get pointer to memory at given 68k location

uint8_t* m68k_get_memory(uint32_t address);

// Resolve file/line/function name from given pc

bool m68k_resolve_file_line(const char** filename, uint32_t* line, const char** function, uint32_t pc);

// Resolve pc given file/line

bool m68k_resolve_pc_line_file(uint32_t* pc, const char* filename, uint32_t line);

// Expose external variable/data to the assembly code

void m68k_expose_data(void* ptr, const char* name);

