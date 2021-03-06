#include "operating_system_utilities.h"

#include <cstdlib>

void kernel_copy_data(unsigned char *destination, unsigned char *source, int size) {
	memcpy(destination, source, (size_t) size);
}

void OSFatal(const char *message) {
	printf("FATAL: ");
	puts(message);
	exit(EXIT_FAILURE);
}