#define EI_NIDENT 16
#define EM_68K 4
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8

#define SHF_EXECINSTR 0x4

#include <stdlib.h>
//#include <Base/Common/Types.h>

#define R_68K_NONE	0
#define R_68K_32	1
#define R_68K_16	2
#define R_68K_8		3
#define R_68K_PC32	4
#define R_68K_PC16	5
#define R_68K_PC8	6
#define R_68K_GOT32	7
#define R_68K_GOT16	8
#define R_68K_GOT8	9
#define R_68K_GOT32O	10
#define R_68K_GOT16O	11
#define R_68K_GOT8O	12
#define R_68K_PLT32	13
#define R_68K_PLT16	14
#define R_68K_PLT8	15
#define R_68K_PLT32O	16
#define R_68K_PLT16O	17
#define R_68K_PLT8O	18
#define R_68K_COPY	19
#define R_68K_GLOB_DAT	20
#define R_68K_JMP_SLOT	21
#define R_68K_RELATIVE	22

/*
 * ELF header.
 */

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* File identification. */
	uint16_t	e_type;		/* File type. */
	uint16_t	e_machine;	/* Machine architecture. */
	uint32_t	e_version;	/* ELF format version. */
	uint32_t	e_entry;	/* Entry point. */
	uint32_t	e_phoff;	/* Program header file offset. */
	uint32_t	e_shoff;	/* Section header file offset. */
	uint32_t	e_flags;	/* Architecture-specific flags. */
	uint16_t	e_ehsize;	/* Size of ELF header in bytes. */
	uint16_t	e_phentsize;	/* Size of program header entry. */
	uint16_t	e_phnum;	/* Number of program header entries. */
	uint16_t	e_shentsize;	/* Size of section header entry. */
	uint16_t	e_shnum;	/* Number of section header entries. */
	uint16_t	e_shstrndx;	/* Section name strings section. */
} Elf32_Ehdr;

/*
 * Section header.
 */

typedef struct {
	uint32_t	sh_name;	/* Section name (index into the section header string table). */
	uint32_t	sh_type;	/* Section type. */
	uint32_t	sh_flags;	/* Section flags. */
	uint32_t	sh_addr;	/* Address in memory image. */
	uint32_t	sh_offset;	/* Offset in file. */
	uint32_t	sh_size;	/* Size in bytes. */
	uint32_t	sh_link;	/* Index of a related section. */
	uint32_t	sh_info;	/* Depends on section type. */
	uint32_t	sh_addralign;	/* Alignment in bytes. */
	uint32_t	sh_entsize;	/* Size of each entry in section. */
} Elf32_Shdr;

/*
 * Program header.
 */

typedef struct {
	uint32_t	p_type;		/* Entry type. */
	uint32_t	p_offset;	/* File offset of contents. */
	uint32_t	p_vaddr;	/* Virtual address in memory image. */
	uint32_t	p_paddr;	/* Physical address (not used). */
	uint32_t	p_filesz;	/* Size of contents in file. */
	uint32_t	p_memsz;	/* Size of contents in memory. */
	uint32_t	p_flags;	/* Access permission flags. */
	uint32_t	p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 */

typedef struct {
	int32_t	d_tag;		/* Entry type. */
	union {
		uint32_t	d_val;	/* Integer value. */
		uint32_t	d_ptr;	/* Address value. */
	} d_un;
} Elf32_Dyn;

/*
 * Relocation entries.
 */

/* Relocations that don't need an addend field. */
typedef struct {
	uint32_t	r_offset;	/* Location to be relocated. */
	uint32_t	r_info;		/* Relocation type and symbol index. */
} Elf32_Rel;

/* Relocations that need an addend field. */
typedef struct {
	uint32_t	r_offset;	/* Location to be relocated. */
	uint32_t	r_info;		/* Relocation type and symbol index. */
	int32_t	r_addend;	/* Addend. */
} Elf32_Rela;

/* Macros for accessing the fields of r_info. */
#define ELF32_R_SYM(info)	((info) >> 8)
#define ELF32_R_TYPE(info)	((unsigned char)(info))

/* Macro for constructing r_info from field values. */
#define ELF32_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

/*
 * Symbol table entries.
 */

typedef struct {
	uint32_t	st_name;	/* String table index of name. */
	uint32_t	st_value;	/* Symbol value. */
	uint32_t	st_size;	/* Size of associated object. */
	unsigned char	st_info;	/* Type and binding information. */
	unsigned char	st_other;	/* Reserved (not used). */
	uint16_t	st_shndx;	/* Section index of symbol. */
} Elf32_Sym;

/* Macros for accessing the fields of st_info. */
#define ELF32_ST_BIND(info)		((info) >> 4)
#define ELF32_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))



