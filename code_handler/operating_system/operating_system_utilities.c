#include "operating_system_utilities.h"

#include <stdlib.h>

void kernelCopyData(unsigned char *destination, unsigned char *source, int size) {
	memcpy(destination, source, (size_t) size);
}

void OSFatal(const char *message) {
	printf("FATAL: ");
	printf(message);
	exit(EXIT_FAILURE);
}