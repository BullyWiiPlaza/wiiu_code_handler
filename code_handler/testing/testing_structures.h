#include <stdbool.h>
#include <memory.h>

typedef struct {
	unsigned int address;
	unsigned int length;
	unsigned char *bytes;
} VirtualMemory;

bool equals(VirtualMemory firstVirtualMemory, VirtualMemory secondVirtualMemory) {
	return memcmp(&firstVirtualMemory, &secondVirtualMemory, sizeof(unsigned int) * 2) == 0
		   && memcmp(firstVirtualMemory.bytes, secondVirtualMemory.bytes, firstVirtualMemory.length) == 0;
}