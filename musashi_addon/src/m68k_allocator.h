#ifndef M68KLINEARALLOCATOR_H
#define M68KLINEARALLOCATOR_H

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include "m68k_types.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct M68KLinearAllocator
{
	uint8_t* start;
	uint8_t* current;
	uint32_t maxSize;
} M68KLinearAllocator;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct M68KLinearAllocatorRewindPoint
{
	uint8_t* pointer;
} M68KLinearAllocatorRewindPoint;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void M68KLinearAllocator_create(void* data, uint32_t size);
void M68KLinearAllocator_reset();

void* M68KLinearAllocator_allocAligned(uint32_t size, uint32_t alignment);
void* M68KLinearAllocator_allocAlignedZero(uint32_t size, uint32_t alignment);

M68KLinearAllocatorRewindPoint M68KLinearAllocator_getRewindPoint();
void M68KLinearAllocator_rewind(M68KLinearAllocatorRewindPoint rewindPoint);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper macros

#define M68KLinearAllocator_alloc(type) (type*)M68KLinearAllocator_allocAligned(sizeof(type), M68K_ALIGNOF(type))
#define M68KLinearAllocator_allocZero(type) (type*)M68KLinearAllocator_allocAlignedZero(sizeof(type), M68K_ALIGNOF(type))
#define M68KLinearAllocator_allocArray(type, count) (type*)M68KLinearAllocator_allocAlignedZero(sizeof(type) * count, \
										  M68K_ALIGNOF(type))
#define M68KLinearAllocator_allocArrayZero(type, count) (type*)M68KLinearAllocator_allocAlignedZero(sizeof(type) * count, M68K_ALIGNOF(type))


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* M68KLinearAllocator_allocString(const char* name);

#endif

