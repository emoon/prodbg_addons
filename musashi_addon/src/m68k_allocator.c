#include "m68k_allocator.h"
#include <string.h>
#include <stdlib.h>

static M68KLinearAllocator g_allocator;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void M68KLinearAllocator_create(void* data, uint32_t size)
{
	M68KLinearAllocator* state = &g_allocator;
	state->start = state->current = (uint8_t*)data;
	state->maxSize = size;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void M68KLinearAllocator_reset()
{
	M68KLinearAllocator* state = &g_allocator;
	state->current = state->start;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
M68KLinearAllocatorRewindPoint M68KLinearAllocator_getRewindPoint() 
{
	M68KLinearAllocator* allocator = &g_allocator;
	M68KLinearAllocatorRewindPoint rewindPoint;
	rewindPoint.pointer = allocator->current;
	return rewindPoint;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void M68KLinearAllocator_rewind(M68KLinearAllocatorRewindPoint rewindPoint)
{
	M68KLinearAllocator* state = &g_allocator;
	state->current = rewindPoint.pointer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* M68KLinearAllocator_allocAligned(uint32_t size, uint32_t alignment)
{
	void* retData;

	// Align the pointer to the current 

	M68KLinearAllocator* state = &g_allocator;
	intptr_t ptr = (intptr_t)state->current;	
	uint32_t bitMask = (alignment - 1);
	uint32_t lowBits = ptr & bitMask;
	uint32_t adjust = ((alignment - lowBits) & bitMask);

	// adjust the pointer to correct alignment

	state->current += adjust;

	// TODO: Handle if we run out of memory (very unlikely)

	retData = (void*)state->current;
	state->current += size;

	return retData;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* M68KLinearAllocator_allocAlignedZero(uint32_t size, uint32_t alignment)
{
	void* mem = M68KLinearAllocator_allocAligned(size, alignment);
	memset(mem, 0, size);
	return mem;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* M68KLinearAllocator_allocString(const char* value)
{
	const size_t len = strlen(value) + 1;
	char* mem = M68KLinearAllocator_allocArray(char, (uint32_t)len); 
	memcpy(mem, value, len);
	return mem;
}

