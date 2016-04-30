#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k_elf_loader.h"
#include "m68k_allocator.h"
#include "m68k_log.h"
#include "m68k_elfstructs.h"
#include <stdint.h>

#define SHT_RELA 4
#define SHT_REL 9
#define SHT_NOTE 7

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum M68KSectionType
{
	M68K_SECTION_BSS,
	M68K_SECTION_CODEDATA,
} M68KSectionType;

typedef struct M68KDebugEntry
{
	uint32_t type;
	uint32_t startLine;
	uint32_t endLine;
	uint32_t pc;
} M68KDebugEntry;

typedef struct M68KSection
{
	const char* name; 				// name of the section
	uint8_t* target; 				// target memory of this section (in 68k memory)
	uint32_t offset; 				// Offset from the start of the memory
	uint32_t size;             	    // size
	uint32_t totalSize;             // Total Size
	M68KSectionType type; 			// Type of section
	M68KDebugEntry* dbgSection; 	// Section for debug info
	Elf32_Rel* relSection;  		// rel-type that needs fixup
	Elf32_Rela* relaSection;  		// rela-type that needs fixup

	uint32_t relSectionCount;
	uint32_t relaSectionCount;
	uint32_t dbgSectionCount;

} M68KSection;

typedef struct M68KStringHashArray
{
	const char** names;
	uint32_t* hashes;
	uint8_t** targets;
	uint32_t count;
} M68KStringHashArray;

typedef struct M68KFile
{
	M68KStringHashArray exportNames;
	M68KSection* sections;
	const char* path;
	const char* sourceFile;
	const char* symNames;
	Elf32_Sym* symbolTable;

	uint32_t sectionCount;
	uint32_t symbolCount;

} M68KFile;

typedef struct M68KProgramInfo
{
	M68KFile* files[512];
	uint32_t fileCount;

} M68KProgramInfo;

typedef struct M68KCodeData
{
	uint8_t* code;
	uint8_t* data;
	uint8_t* bss;
	uint8_t* memStart;

	M68KProgramInfo progInfo;

	uint32_t codeSize;
	uint32_t dataSize;
	uint32_t bssSize;
	uint32_t totalSize;
} M68KCodeData;

static M68KCodeData g_prog;
static M68KProgramInfo g_progInfo;
unsigned char* g_68kmem;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t* loadFileToMemory(const char* filename)
{
	int size;
	uint8_t* memory = 0;
	FILE* f = fopen(filename, "rb");

	if (!f)
	{
		m68k_log(M68K_LOG_ERROR, "Unable to open %s for reading\n", filename);
		return 0;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	memory = malloc(size);

	if (!memory)
	{
		m68k_log(M68K_LOG_ERROR, "Unable to allocate %d bytes to read file %s\n", size, filename);
		return 0;
	}

	fread(memory, 1, size, f);
	fclose(f);

	return memory;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// taken from:
// http://blade.nagaokaut.ac.jp/cgi-bin/scat.rb/ruby/ruby-talk/142054
//
// djb  :: 99.8601 percent coverage (60 collisions out of 42884)
// elf  :: 99.5430 percent coverage (196 collisions out of 42884)
// sdbm :: 100.0000 percent coverage (0 collisions out of 42884) (this is the algo used)
// ...

static uint32_t quickHash(const char* string)
{
	uint32_t c;
	uint32_t hash = 0;

	const uint8_t* str = (const uint8_t*)string;

	while ((c = *str++))
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash & 0x7FFFFFFF;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE uint16_t swap16(uint16_t i)
{
	return (uint16_t) (((((unsigned int) i) & 0xff00) >> 8)
					 | ((((unsigned int) i) & 0xff) << 8));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE uint32_t swap32(uint32_t i)
{
	return (int) (((((unsigned int) i) & 0xff000000) >> 24)
				| ((((unsigned int) i) & 0xff0000) >> 8)
				| ((((unsigned int) i) & 0xff00) << 8)
				| ((((unsigned int) i) & 0xff) << 24));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE size_t alignUp(size_t value, uint32_t alignment)
{
	intptr_t ptr = value;
	uint32_t bitMask = (alignment - 1);
	uint32_t lowBits = ptr & bitMask;
	uint32_t adjust = ((alignment - lowBits) & bitMask);
	return value + adjust;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE void swapHeader(Elf32_Ehdr* header)
{
	header->e_type = swap16(header->e_type);
	header->e_machine = swap16(header->e_machine);
	header->e_version = swap32(header->e_version);
	header->e_entry = swap32(header->e_entry);
	header->e_phoff = swap32(header->e_phoff);
	header->e_shoff = swap32(header->e_shoff);
	header->e_flags = swap32(header->e_flags);
	header->e_ehsize = swap16(header->e_ehsize);
	header->e_phentsize = swap16(header->e_phentsize);
	header->e_phnum = swap16(header->e_phnum);
	header->e_shentsize = swap16(header->e_shentsize);
	header->e_shnum = swap16(header->e_shnum);
	header->e_shstrndx = swap16(header->e_shstrndx);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE void swapSection(Elf32_Shdr* section)
{
	section->sh_name = swap32(section->sh_name);
	section->sh_type = swap32(section->sh_type);
	section->sh_flags = swap32(section->sh_flags);
	section->sh_addr = swap32(section->sh_addr);
	section->sh_offset = swap32(section->sh_offset);
	section->sh_size = swap32(section->sh_size);
	section->sh_link = swap32(section->sh_link);
	section->sh_info = swap32(section->sh_info);
	section->sh_addralign = swap32(section->sh_addralign);
	section->sh_entsize = swap32(section->sh_entsize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE void swapSymtabEntry(Elf32_Sym* symEntry)
{
	symEntry->st_name = swap32(symEntry->st_name);
	symEntry->st_value = swap32(symEntry->st_value);
	symEntry->st_size = swap32(symEntry->st_size);
	symEntry->st_shndx = swap16(symEntry->st_shndx);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE void swapRela(Elf32_Rela* entry)
{
	entry->r_offset = swap32(entry->r_offset);
	entry->r_info = swap32(entry->r_info);
	entry->r_addend = swap32(entry->r_addend);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
static M68K_INLINE void swapRel(Elf32_Rel* entry)
{
	entry->r_offset = swap32(entry->r_offset);
	entry->r_info = swap32(entry->r_info);
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void printSection(uint32_t index, const Elf32_Shdr* section, const char* names)
{
	m68k_log(M68K_LOG_DEBUG, "%04d type = %04x flags = %04x addr = %04x offset = %08x size = %08x link = %04x info = %04x addralign = %02x entsize = %04x name = %s\n",
		index,
		section->sh_type, section->sh_flags, section->sh_addr, section->sh_offset, section->sh_size,
		section->sh_link, section->sh_info, section->sh_addralign, section->sh_entsize, &names[section->sh_name]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void printSymtab(uint32_t i, const Elf32_Sym* symEntry, const char* names)
{
    m68k_log(M68K_LOG_DEBUG, "%04d value = %08x size = %04d info = (%04d/%04d) sec = %04x name = %s\n",
		i, symEntry->st_value, symEntry->st_size, symEntry->st_info >> 4,
		symEntry->st_info & 0xf, symEntry->st_shndx, &names[symEntry->st_name]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void printRela(uint32_t index, const Elf32_Rela* rela)
{
	m68k_log(M68K_LOG_DEBUG, "%04d offset = %08x info = (%04d/%04d) append = %08x\n",
		index, rela->r_offset, ELF32_R_SYM(rela->r_info), ELF32_R_TYPE(rela->r_info), rela->r_addend);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
static void printDbgSection(const M68KDebugEntry* entries, int count)
{
	for (int i = 0; i < count; ++i)
	{
		m68k_log(M68K_LOG_DEBUG, "debugEntry %04d type = %04d startLine = %04d endLine = %04d pc = %08x\n",
				i, entries[i].type, entries[i].startLine, entries[i].endLine, entries[i].pc);
	}
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE const char* getNameOffset(uint8_t* fileBuffer, const char* names, Elf32_Shdr* sections, uint32_t sectionCount)
{
	uint32_t i;

	for (i = 0; i < sectionCount; ++i)
	{
		if (sections[i].sh_type == SHT_STRTAB && !strcmp(".strtab", &names[sections[i].sh_name]))
			return (const char*)(fileBuffer + sections[i].sh_offset);
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE void swapDatas(
		M68KFile* file,
		uint8_t* fileBuffer,
		const char* sectionNames,
		const char* names,
		Elf32_Shdr* sections,
		uint32_t sectionCount)
{
	uint32_t i;

	for (i = 0; i < sectionCount; ++i)
	{
		const Elf32_Shdr* section = &sections[i];

		m68k_log(M68K_LOG_DEBUG, "Section %s\n", &sectionNames[section->sh_name]);

		switch (section->sh_type)
		{
			case SHT_SYMTAB :
			{
				uint32_t k;
				uint32_t size = section->sh_size / section->sh_entsize;
				Elf32_Sym* symtab_entries = (Elf32_Sym*)((fileBuffer + section->sh_offset));

				file->symbolTable = symtab_entries;
				file->symbolCount = size;

				for (k = 0; k < size; ++k)
				{
					swapSymtabEntry(&symtab_entries[k]);
					printSymtab(k, &symtab_entries[k], names);
				}

				break;
			}

			case SHT_RELA :
			{
				uint32_t k;
				uint32_t size = section->sh_size / section->sh_entsize;
				Elf32_Rela* rela = (Elf32_Rela*)(fileBuffer + section->sh_offset);

				for (k = 0; k < size; ++k)
				{
					swapRela(&rela[k]);
					printRela(k, &rela[k]);
				}

				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parses all the sections/symtabs/etc

static void swapElfObject(M68KFile* file, uint8_t* fileBuffer, const char* sectionNames, const Elf32_Ehdr* header)
{
	uint32_t section_count = header->e_shnum;
	Elf32_Shdr* sections = (Elf32_Shdr*)(fileBuffer + header->e_shoff);
	const char* stringTable;
	uint32_t i;

	for (i = 0; i < section_count; ++i)
	{
		swapSection(&sections[i]);
		printSection(i, &sections[i], sectionNames);
	}

	stringTable = getNameOffset(fileBuffer, sectionNames, sections, section_count);
	swapDatas(file, fileBuffer, sectionNames, stringTable, sections, section_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static M68K_INLINE uint32_t getOffset(const uint8_t* end, const uint8_t* start)
{
	return (uint32_t)(((uintptr_t)end) - ((uintptr_t)start));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void findRelocSections(
	M68KSection* section,
	uint8_t* fileBuffer,
	Elf32_Shdr* elfSections,
	const char* names,
	uint32_t elfSectionCount)
{
	char relaName[512];
	char relName[512];
	char dbgName[512];
	uint32_t i;

	sprintf(relaName, ".rela%s", section->name);
	sprintf(dbgName, ".dbg%s", section->name);
	sprintf(relName, ".rel%s", section->name);

	for (i = 0; i < elfSectionCount; ++i)
	{
		const Elf32_Shdr* elfSection = &elfSections[i];

		if (elfSection->sh_type == SHT_RELA &&
		   !strcmp(relaName, &names[elfSection->sh_name]))
		{
			section->relaSection = (Elf32_Rela*)(fileBuffer + elfSection->sh_offset);
			section->relaSectionCount = elfSection->sh_size / elfSection->sh_entsize;
		}

		if (elfSection->sh_type == SHT_REL &&
		   !strcmp(relName, &names[elfSection->sh_name]))
		{
			section->relSection = (Elf32_Rel*)(fileBuffer + elfSection->sh_offset);
			section->relSectionCount = elfSection->sh_size / elfSection->sh_entsize;
		}

		if (elfSection->sh_type == SHT_NOTE &&
		   !strcmp(dbgName, &names[elfSection->sh_name]))
		{
			section->dbgSection = (M68KDebugEntry*)(fileBuffer + elfSection->sh_offset);
			section->dbgSectionCount = elfSection->sh_size / elfSection->sh_entsize;
			//printDbgSection(section->dbgSection, section->dbgSectionCount);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate and copy code/data to the sections

static void allocateSections(
	M68KFile* file,
	M68KCodeData* codeData,
	uint8_t* fileBuffer,
	const char* sectionNames,
	Elf32_Shdr* elfSections,
	uint32_t elfSectionCount)
{
	uint32_t i;

	for (i = 0; i < elfSectionCount; ++i)
	{
		const Elf32_Shdr* elfSection = &elfSections[i];
		M68KSection* section = &file->sections[i];
		uint32_t size = alignUp(elfSection->sh_size + 4 * 1024, 4 * 1024);

		if (elfSection->sh_type == SHT_PROGBITS ||
			elfSection->sh_type == SHT_NOBITS)
		{
			uint8_t* target;
			uint32_t code_offset, dbg_entry_count, d;

			section->name = M68KLinearAllocator_allocString(&sectionNames[elfSection->sh_name]);

			findRelocSections(section, fileBuffer, elfSections, sectionNames, elfSectionCount);

			if (elfSection->sh_type == SHT_PROGBITS)
			{
				if (strstr("data", section->name))
				{
					section->target = target = codeData->data;
					codeData->data += size;
				}
				else
				{
					section->target = target = codeData->code;
					codeData->code += size;
				}

				memcpy(target, fileBuffer + elfSection->sh_offset, elfSection->sh_size);
				memset(target + elfSection->sh_size, 0, size - elfSection->sh_size);

				code_offset = getOffset(section->target, codeData->memStart);

				m68k_log(M68K_LOG_DEBUG, "Copy/Alloc target = %016lx (%08x) | %08d bytes (original size %08d) rela = %016lx (%04d) dbg = %016lx (%04d) for section %s\n",
					(uintptr_t)target,
					code_offset,
					size,
					elfSection->sh_size, (uintptr_t)section->relaSection, section->relaSectionCount,
					(uintptr_t)section->dbgSection, section->dbgSectionCount, section->name);

				// if we have a debug section here we need to fixup the pc entries as well

				for (d = 0, dbg_entry_count = section->dbgSectionCount; d < dbg_entry_count; ++d)
					section->dbgSection[d].pc += code_offset;

				section->type = M68K_SECTION_CODEDATA;
			}
			else
			{
				section->target = target = codeData->bss;
				section->type = M68K_SECTION_BSS;
				section->offset = getOffset(section->target, codeData->memStart);

				m68k_log(M68K_LOG_DEBUG, "Allocated target = %016lx (%08x) | %08d bytes (original size %08d) rela = %016lx (%04d) rel = %016lx (%04d) for section %s\n",
						(uintptr_t)target,
						section->offset,
						size, elfSection->sh_size, (uintptr_t)section->relaSection, section->relaSectionCount,
						(uintptr_t)section->relSection, section->relSectionCount, section->name);

				memset(target, 0, size);
				codeData->bss += size;
			}

			section->offset = getOffset(target, codeData->memStart);
			section->size = elfSection->sh_size;
			section->totalSize = size;
		}

		// todo: verify so we don't run of of space here
	}

	file->sectionCount = elfSectionCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void setupExportNames(M68KFile* file, const char* symNames)
{
	uint32_t i, k = 0, count = file->symbolCount;
	uint32_t exportCount = 0;

	file->symNames = symNames;

	for (i = 0; i < count; ++i)
	{
		const Elf32_Sym* symbol = &file->symbolTable[i];
		if (((symbol->st_info >> 4) == 1) && symbol->st_shndx > 0)
			exportCount++;
	}

	file->exportNames.names = M68KLinearAllocator_allocArray(const char*, exportCount);
	file->exportNames.hashes = M68KLinearAllocator_allocArray(uint32_t, exportCount);
	file->exportNames.targets = M68KLinearAllocator_allocArray(uint8_t*, exportCount);
	file->exportNames.count = exportCount;

	for (i = 0; i < count; ++i)
	{
		const Elf32_Sym* symbol = &file->symbolTable[i];

		if (((symbol->st_info >> 4) == 1) && symbol->st_shndx > 0)
		{
			file->exportNames.names[k] = &symNames[symbol->st_name];
			file->exportNames.hashes[k] = quickHash(&symNames[symbol->st_name]);
			file->exportNames.targets[k] = file->sections[symbol->st_shndx].target + symbol->st_value;

			m68k_log(M68K_LOG_DEBUG, "Inserted %08x offset = %p into exportNames (%s)\n",
				file->exportNames.hashes[k], file->exportNames.targets[k], file->exportNames.names[k]);
			++k;
		}
		else if ((symbol->st_info & 0xf) == 4)
		{
			file->sourceFile = &symNames[symbol->st_name];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t* resolveSymbolAddress(uint32_t symIndex, M68KFile* file, M68KFile** files, uint32_t fileCount)
{
	const Elf32_Sym* symbol = &file->symbolTable[symIndex];
	const char* symNames = file->symNames;
	const uint32_t bind = ELF32_ST_BIND(symbol->st_info);
	const int16_t section = (int16_t)symbol->st_shndx;

	// Verify that this is a local section (and not external one)

	if (section > 0)
	{
		return file->sections[symbol->st_shndx].target + symbol->st_value;
	}
	else if (section == 0 && bind == 1)  // make sure it's a global bind
	{
		const char* name = &symNames[symbol->st_name];
		M68KStringHashArray* hashArray;
		uint32_t i, k, count, nameHash = quickHash(name);

		for (i = 0; i < fileCount; ++i)
		{
            M68KFile* currentFile = files[i];

			if (currentFile == file) // skip seaching in the current file
				continue;

			hashArray = &currentFile->exportNames;

			for (k = 0, count = hashArray->count; k < count; ++k)
			{
				if (nameHash == hashArray->hashes[k])
				{
					m68k_log(M68K_LOG_DEBUG, "was looking for %s and found %s\n", name, hashArray->names[k]);
					return hashArray->targets[k];
				}
			}
		}

		m68k_log(M68K_LOG_ERROR, "%s : Unable to resolve symbol %s\n", file->path, name);
		exit(-1);
	}

	m68k_log(M68K_LOG_ERROR, ":(\n");
	exit(-1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void write_u32(uint8_t* target, uint32_t offset, uint32_t address)
{
	target[offset + 0] = (address >> 24) & 0xff;
	target[offset + 1] = (address >> 16) & 0xff;
	target[offset + 2] = (address >> 8) & 0xff;
	target[offset + 3] = (address >> 0) & 0xff;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void write_u16(uint8_t* target, uint32_t offset, uint32_t address)
{
	target[offset + 0] = (address >> 8) & 0xff;
	target[offset + 1] = (address >> 0) & 0xff;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool resolveSymbols(M68KFile* file, M68KFile** files, uint32_t fileCount)
{
	uint32_t i, sectionCount = file->sectionCount;

	for (i = 0; i < sectionCount; ++i)
	{
		const M68KSection* section = &file->sections[i];
		uint8_t* target = section->target;

		// No section to fixup here so just go on

		if (section->relSection)
		{
			m68k_log(M68K_LOG_ERROR, "rel section not handled!\n");
			continue;
		}

		if (section->relaSection)
		{
			uint32_t k, relaCount = section->relaSectionCount;
			const Elf32_Rela* relaSection = section->relaSection;

			for (k = 0; k < relaCount; ++k)
			{
				const Elf32_Rela* rela = &relaSection[k];
				uint32_t type = ELF32_R_TYPE(rela->r_info);
				uint32_t symIndex = ELF32_R_SYM(rela->r_info);

				switch (type)
				{
					case R_68K_32 :
					{
						uint32_t* base = (uint32_t*)resolveSymbolAddress(symIndex, file, files, fileCount);
						write_u32(target, rela->r_offset, getOffset((uint8_t*)base, g_prog.memStart) + rela->r_addend);
						//target[rela->r_offset >> 2] = (uint32_t)(base + rela->r_addend);
						break;
					}

					case R_68K_16 :
					{
						uint32_t* base = (uint32_t*)resolveSymbolAddress(symIndex, file, files, fileCount);
						write_u16(target, rela->r_offset, getOffset((uint8_t*)base, g_prog.memStart) + rela->r_addend);
						break;
					}

					// Relative from PC (for example a bra)
					// target = data/function that is being called (should always be code)

					/*
					case R_68K_PC32 :
					{
						//int32_t* base = (int32_t*)resolveSymbolAddress(symIndex, file, files, fileCount);
						//int32_t* secTarget = (int32_t*)(section->target + (rela->r_offset >> 2));
						//secTarget = (int32_t)((uintptr_t)(target) - ((uintptr_t)secTarget));
						break;
					}

					case R_68K_PC16 :
					{
						int16_t* target = (int16_t*)resolveSymbolAddress(symIndex, file, files, fileCount);
						int16_t* secTarget = (int16_t*)(section->target + (rela->r_offset >> 1));
						int32_t offset = (int32_t)((uintptr_t)(target) - ((uintptr_t)secTarget));

						// Make sure jump is within 16-bit range

						if (((uint32_t)offset) > 65536)
						{
							m68k_log(M68K_LOG_ERROR, "%s : jump outside 16-bit range\n", file->path);
							exit(-1);
						}

						*secTarget = (int16_t)offset;

						break;
					}
					*/

					default :
					{
						m68k_log(M68K_LOG_ERROR, "Unupported relatype :( %d\n", type);
						exit(-1);
					}
				}
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool m68k_elf_link()
{
	uint32_t i, fileCount = g_progInfo.fileCount;

	for (i = 0;i < fileCount; ++i)
	{
		M68KFile* file = g_progInfo.files[i];

		if (!resolveSymbols(file, g_progInfo.files, fileCount))
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int load_file(M68KFile* file, M68KCodeData* codeData, const char* filename)
{
	uint8_t* file_buffer = loadFileToMemory(filename);
	Elf32_Ehdr* elf_header = (Elf32_Ehdr*)file_buffer;
	Elf32_Shdr* sections = 0;
	const char* section_strings;

	if (!file_buffer)
		return 0;

	swapHeader(elf_header);

	if (elf_header->e_machine != EM_68K)
	{
		m68k_log(M68K_LOG_DEBUG, "%s\n", elf_header->e_ident);
		m68k_log(M68K_LOG_ERROR, "File %s is not the correct filetype. Expected %d but got %d\n", filename, EM_68K, elf_header->e_machine);
		goto error;
	}

	file->path = M68KLinearAllocator_allocString(filename);

	// Swap all the sections

	sections = (Elf32_Shdr*)(file_buffer + elf_header->e_shoff);
	section_strings = (const char*)(file_buffer + swap32(sections[elf_header->e_shstrndx].sh_offset));
	swapElfObject(file, file_buffer, section_strings, elf_header);

	// Allocate space in 68k memory and add tracking of the sectios

	file->sections = M68KLinearAllocator_allocArrayZero(M68KSection, elf_header->e_shnum);
	allocateSections(file, codeData, file_buffer, section_strings, sections, elf_header->e_shnum);

	setupExportNames(file, getNameOffset(file_buffer, section_strings, sections, elf_header->e_shnum));

error:

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_elf_load(const char* filename)
{
	uint32_t i, file_count = g_progInfo.fileCount;
	int ret;
	M68KFile* file;

	m68k_log_set_level(M68K_LOG_INFO);
	m68k_log(M68K_LOG_INFO, "Loading elf file %s\n", filename);

	if (!filename)
	{
		m68k_log(M68K_LOG_ERROR, "Unable to load file as it's a null pointer!\n");
		return -1;
	}

	for (i = 0; i < file_count; ++i)
	{
		if (!strcmp(filename, g_progInfo.files[i]->path))
		{
			// todo: handle reload of the file which currently isn't supported
			m68k_log(M68K_LOG_INFO, "Reloading of files isn't currently supported. Tried to load file: %s which has already been loaded", filename);
			return -1;
		}
	}

	file = M68KLinearAllocator_allocZero(M68KFile);
	if ((ret = load_file(file, &g_prog, filename)) != 0)
		return ret;

	g_progInfo.files[g_progInfo.fileCount] = file;

	if (g_progInfo.fileCount >= 512)
	{
		m68k_log(M68K_LOG_ERROR, "Reached max number of loaded files (512)\n");
		return -1;
	}

	g_progInfo.fileCount++;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int m68k_code_init(void* memory, int codeSize, int dataSize, int bssSize)
{
	g_prog.code = memory;
	g_prog.data = g_prog.code + codeSize;
	g_prog.bss = g_prog.data + dataSize;
	g_prog.memStart = g_prog.code;
	g_prog.codeSize = codeSize;
	g_prog.dataSize = dataSize;
	g_prog.bssSize = bssSize;
	g_prog.totalSize = codeSize + dataSize + bssSize;
  	g_68kmem = memory;

	M68KLinearAllocator_create(malloc(5 * 1024 * 1024), 5 * 1024 * 1024);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Put stack at the end of bss

extern uint32_t m68k_get_sp()
{
    return (uint32_t)(uintptr_t)(g_prog.memStart + g_prog.codeSize +  g_prog.dataSize +  g_prog.bssSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Hashmap of the names here? Likely not an issue really

int m68k_find_symbol(const char* function)
{
	uint32_t i, file_count = g_progInfo.fileCount;
	uint32_t nameHash = 0;

	if (!function)
	{
		m68k_log(M68K_LOG_ERROR, "Can't call function with null parameter as name\n");
		return -1;
	}

	nameHash = quickHash(function);

	for (i = 0; i < file_count; ++i)
	{
		uint32_t k, hash_count;

		M68KFile* file = g_progInfo.files[i];

		hash_count = file->exportNames.count;

		for (k = 0; k < hash_count; ++k)
		{
			if (nameHash == file->exportNames.hashes[k])
			{
				// sanity check with string also

				if (!strcmp(function, file->exportNames.names[k]))
					//return (int)((uintptr_t)file->exportNames.targets[k] - (uintptr_t)g_prog.memStart);
					return (int)((uintptr_t)file->exportNames.targets[k]);
			}
		}
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Use local names as well (not just exported)

const char* getFunctionName(const M68KFile* file, uint32_t pc, uintptr_t memStart)
{
	uint32_t i, hash_count = file->exportNames.count;

	for (i = 0; i < hash_count; ++i)
	{
		uint32_t target_pc = (uint32_t)((uintptr_t)file->exportNames.targets[i] - memStart);

		if (target_pc == pc)
			return file->exportNames.names[i];
	}

	return "";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_find_labels(M68KLabelAddress* labels, uint32_t* count, uint32_t pcStart, uint32_t pcEnd)
{
	uint32_t i;
	uint32_t current_count = 0;
	uint32_t max_count = *count;
	uint32_t file_count = g_progInfo.fileCount;
	uintptr_t mem_start = (uintptr_t)g_prog.memStart;

	if (!labels)
	{
		m68k_log(M68K_LOG_ERROR, "Unable to fill in labels if labels are null\n");
		return;
	}

	// Scan all files for labels within the pcRange

	for (i = 0; i < file_count; ++i)
	{
		uint32_t k, hash_count;

		M68KFile* file = g_progInfo.files[i];

		hash_count = file->exportNames.count;

		for (k = 0; k < hash_count; ++k)
		{
			uint32_t pc = (uint32_t)((uintptr_t)file->exportNames.targets[k] - mem_start);

			if (!(pc >= pcStart && pc <= pcEnd))
				continue;

			if (current_count >= max_count)
			{
				m68k_log(M68K_LOG_INFO, "Unable to add labels as buffer is too small\n");
				continue;
			}

			labels[current_count].address = pc;
			labels[current_count].name = file->exportNames.names[k];
			current_count++;
		}
	}

	*count = current_count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const M68KSection* findSectionForPC(const M68KFile* file, uint32_t pc)
{
	uint32_t i, section_count = file->sectionCount;

	for (i = 0; i < section_count; ++i)
	{
		const M68KSection* section = &file->sections[i];

		uint32_t start_pc = section->offset;
		uint32_t end_pc = start_pc + section->size;

		if (pc >= start_pc && pc < end_pc)
			return section;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t getLine(const M68KSection* section, uint32_t pc)
{
	uint32_t i, dbg_count;
	const M68KDebugEntry* dbgSection = section->dbgSection;

	if (!dbgSection)
		return 0;

	for (i = 0, dbg_count = section->dbgSectionCount; i < dbg_count; ++i)
	{
		if (dbgSection[i].pc == pc && dbgSection[i].type == 2)
			return dbgSection[i].startLine;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Merge with the code above?

bool m68k_resolve_file_line(const char** filename, uint32_t* line, const char** function, uint32_t pc)
{
	uint32_t i;
	uint32_t file_count = g_progInfo.fileCount;
	uintptr_t mem_start = (uintptr_t)g_prog.memStart;

	// Scan all files for labels within the pcRange

	for (i = 0; i < file_count; ++i)
	{
		uint32_t k, export_count;

		M68KFile* file = g_progInfo.files[i];

		// TODO: Do a check if the pc is with in the scope of the file at all to not need to check each symbol

		for (k = 0, export_count = file->exportNames.count; k < export_count; ++k)
		{
			const M68KSection* section;

			if (!(section = findSectionForPC(file, pc)))
				continue;

			*filename = file->sourceFile;
			*line = getLine(section, pc);
			*function = getFunctionName(file, pc, mem_start);
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Find the PC given filename and line

bool m68k_resolve_pc_line_file(uint32_t* pc, const char* filename, uint32_t line)
{
	uint32_t i;
	uint32_t file_count = g_progInfo.fileCount;

	for (i = 0; i < file_count; ++i)
	{
		uint32_t k, section_count;

		M68KFile* file = g_progInfo.files[i];

		if (strcmp(file->sourceFile, filename))
			continue;

		for (k = 0, section_count = file->sectionCount; k < section_count; ++k)
		{
			uint32_t s, dbg_section_count;
			const M68KSection* section = &file->sections[k];

			if (!section->dbgSection)
				continue;

			for (s = 0, dbg_section_count = section->dbgSectionCount; s < dbg_section_count; ++s)
			{
				const M68KDebugEntry* entry = &section->dbgSection[s];

				if (line >= entry->startLine && line < entry->endLine)
				{
					*pc = entry->pc;
					return true;
				}
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t* m68k_get_memory(uint32_t address)
{
	return g_prog.memStart + address;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t m68k_get_ptr_offset(const void* ptr, const char* ident)
{
	uint8_t* p = (uint8_t*)ptr;
	//uint8_t* start = g_prog.memStart;
	//uint8_t* end = g_prog.memStart + g_prog.totalSize;

	// allow null pointers
	if (!p)
		return 0;

	//NEWAGE_ASSERT_DESC_FORMAT(p >= start && p < end,
	//	"Parameter: \"%s\" is not the memory range ptr %p (range %p - %p), make sure to use only memory allocated from sysAlloc (not on stack)",
	//	ident, p, start, end);

	return getOffset(ptr, g_prog.memStart);
}


